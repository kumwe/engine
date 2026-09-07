<?php
/** Test-only unchanged App oracle and actual Zend boundary benchmark. Never installed. */
declare(strict_types=1);

use Kumwe\App\BusinessDefinition\Domain\{CanonicalDefinitionJson, EntityTypeDefinition, Expression};
use Kumwe\App\BusinessRecord\Application\{RecordRuleValidator, RecordValueCodec};
use Kumwe\App\BusinessRecord\Application\Exception\BusinessRecordValidationFailed;
use Kumwe\App\BusinessRecord\Domain\RecordValueGuard;
use Kumwe\App\BusinessRecord\Infrastructure\Security\SodiumSecretCipher;
use Kumwe\Conversion\Decimal\{ExactDecimal, ExactDecimalArithmetic};

$config = json_decode(file_get_contents($argv[1]), true, 512, JSON_THROW_ON_ERROR);
require $config['autoload'];
spl_autoload_register(static function (string $name) use ($config): void {
    foreach (['Kumwe\\App\\' => $config['app'] . '/src/', 'Kumwe\\Extension\\' => $config['sdk'] . '/src/'] as $prefix => $root) {
        if (str_starts_with($name, $prefix)) {
            $file = $root . str_replace('\\', '/', substr($name, strlen($prefix))) . '.php';
            if (is_file($file)) { require $file; }
            return;
        }
    }
});
const ID = '018f4f24-98d8-7ad4-8f3f-38c909178b6b';
const JSON_FLAGS = JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE | JSON_PRESERVE_ZERO_FRACTION | JSON_THROW_ON_ERROR;
$native = $config['backend'] === 'native';
$runtime = $native ? new Kumwe\Engine\Runtime() : null;
$codec = new RecordValueCodec(new SodiumSecretCipher('benchmark', str_repeat('x', 32)));
$rules = new RecordRuleValidator($codec);
$compute = new ReflectionMethod($rules, 'compute');
$validate = new ReflectionMethod($rules, 'validate');
$reportService = (new ReflectionClass(Kumwe\App\BusinessReporting\Application\ReportService::class))->newInstanceWithoutConstructor();
$materialize = new ReflectionMethod($reportService, 'materialize');
$cache = [];

function fixture(string $path, string $id): array
{
    $data = json_decode(file_get_contents($path), true, 512, JSON_THROW_ON_ERROR);
    foreach ($data['fixtures'] as $item) { if ($item['id'] === $id) { return $item; } }
    throw new RuntimeException('Missing fixed oracle fixture: ' . $id);
}
function sorted(mixed $value): mixed
{
    if ($value instanceof stdClass) { $value = (array) $value; }
    if (!is_array($value)) { return $value; }
    if (!array_is_list($value)) { ksort($value, SORT_STRING); }
    foreach ($value as &$item) { $item = sorted($item); }
    return $value;
}
function rss(): array
{
    $text = file_get_contents('/proc/self/status');
    preg_match('/^VmRSS:\s+(\d+) kB/m', $text, $resident);
    preg_match('/^VmHWM:\s+(\d+) kB/m', $text, $peak);
    return ['rss_bytes' => ((int) ($resident[1] ?? 0)) * 1024, 'peak_rss_bytes' => ((int) ($peak[1] ?? 0)) * 1024];
}
function findings(array $items): array
{
    return array_map(static fn ($v) => ['field' => $v->field, 'code' => $v->code], $items);
}
function project(array $definition): array
{
    $fields = [];
    foreach ($definition['fields'] as $field) {
        $out = [];
        foreach (['handle', 'required', 'nullable', 'formula', 'validators', 'type', 'precision', 'scale', 'length', 'normalizers'] as $key) {
            if (array_key_exists($key, $field)) { $out[$key] = $field[$key]; }
        }
        $fields[] = $out;
    }
    return ['fields' => $fields, 'invariants' => $definition['record_invariants'] ?? []];
}
function setup(array $job): array
{
    global $config;
    $profile = $job['profile']; $size = $job['size']; $hostile = $job['shape'] === 'hostile';
    if (!in_array($profile, ['decimal', 'formula', 'document', 'preparation', 'report', 'canonical'], true)
        || !is_int($size) || $size < 1 || $size > 4096) { throw new RuntimeException('Invalid workload.'); }
    $root = $config['engine'] . '/corpus/';
    $literal = static fn ($n) => ['op' => 'literal', 'type' => 'integer', 'value' => $n];
    $expression = ['op' => 'add', 'type' => 'integer', 'args' => [
        ['op' => 'field', 'type' => 'integer', 'field' => 'amount'], $literal(2)]];
    $width = $size <= 1 ? 1 : ($size <= 32 ? 4 : ($size <= 256 ? 16 : 64));
    $data = ['profile' => $profile, 'size' => $size, 'hostile' => $hostile, 'width' => $width];
    if ($profile === 'formula') {
        for ($i = 1; $i < min($width, 10); ++$i) { $expression = ['op' => 'add', 'type' => 'integer', 'args' => [$expression, $literal(2)]]; }
        $data += ['program' => $expression, 'native_profile' => 'formula-draft/1', 'corpus' => 'definition/formula-v1.json'];
    } elseif ($profile === 'document' || $profile === 'preparation') {
        $fixture = fixture($root . 'document/preparation-v1.json', 'create_conditions_after_computation');
        $definition = $fixture['definition'];
        $base = $definition['fields'][1]; $definition['fields'] = [$definition['fields'][0], $definition['fields'][2]];
        for ($i = 0; $i < $width; ++$i) {
            $field = $base; $field['handle'] = 'name_' . $i; $field['validators'] = [['rule' => 'min_length', 'value' => 1]];
            $definition['fields'][] = $field;
        }
        $data['definition'] = $definition;
        $program = project($definition);
        if ($profile === 'preparation') {
            $fields = [];
            foreach ($definition['fields'] as $field) {
                $fields[] = ['handle' => $field['handle'], 'identity' => $field['type'] === 'core.uuid', 'sequence' => false,
                    'computed' => $field['computed'] || $field['formula'] !== null, 'server_only' => $field['server_only'],
                    'read_only' => $field['read_only'], 'immutable_after_create' => $field['immutable_after_create'],
                    'default' => ['value' => null, 'valid' => true], 'visibility_condition' => $field['visibility_condition'],
                    'editability_condition' => $field['editability_condition']];
            }
            $program = ['fields' => $fields, 'validation' => $program];
        }
        $data += ['program' => $program, 'native_profile' => 'normalized-' . $profile . '-draft/1',
            'corpus' => $profile === 'document' ? 'document/document-profile-v1.json' : 'document/preparation-v1.json'];
    } elseif ($profile === 'report') {
        $fixture = fixture($root . 'reporting/materialization-v1.json', 'group_insertion_order');
        $data['definition'] = $fixture['plan'];
        $data['program'] = array_intersect_key($fixture['plan'], array_flip(['columns', 'groups', 'aggregates', 'formulas', 'sorts']));
        $data += ['native_profile' => 'report-materialization-draft/1', 'corpus' => 'reporting/materialization-v1.json'];
    } elseif ($profile === 'canonical') {
        $data += ['native_profile' => 'kumwe-canonical-json/generic-v1', 'corpus' => 'canonical/generic-v1.json'];
    } else {
        $data += ['native_profile' => 'decimal-batch-draft/1', 'corpus' => 'decimal/decimal-v1.tsv'];
    }
    $data['corpus_digest'] = hash_file('sha256', $root . $data['corpus']);
    return $data;
}
function compile_case(array $data): array
{
    global $native;
    $out = $data;
    if (isset($data['definition'])) {
        $out['php_plan'] = $data['profile'] === 'report'
            ? Kumwe\App\BusinessReporting\Domain\ReportDefinition::fromArray($data['definition'])
            : EntityTypeDefinition::fromArray($data['definition']);
    } elseif ($data['profile'] === 'formula') { $out['php_plan'] = Expression::fromArray($data['program']); }
    if ($native && isset($data['program'])) {
        // Runtime lifetime owns this plan; cold phases include its destruction too.
        $out['runtime'] = new Kumwe\Engine\Runtime();
        $out['native_plan'] = $out['runtime']->compile(['wire_version' => 1, 'profile' => $data['native_profile'],
            'corpus_digest' => $data['corpus_digest'], 'program' => json_encode($data['program'], JSON_FLAGS)]);
    }
    return $out;
}
function run_case(array $case, string $phase): mixed
{
    global $native, $runtime, $codec, $rules, $compute, $validate, $materialize, $reportService, $opaqueResults;
    $profile = $case['profile']; $size = $case['size']; $hostile = $case['hostile'];
    if ($profile === 'canonical') {
        // Shared App/native subset: no floats, <=512 items per array, depth <32.
        $input = []; $chunk = [];
        for ($i = 0; $i < $size; ++$i) {
            $chunk['key_' . (4096 - $i)] = $i;
            if (count($chunk) === 256) { $input[] = $chunk; $chunk = []; }
        }
        if ($chunk !== []) { $input[] = $chunk; }
        if ($hostile) { $input[0]['key_4096'] = "\xff"; }
        try {
            if ($native) {
                $result = $runtime->execute(['wire_version' => 1, 'profile' => $case['native_profile'],
                    'corpus_digest' => $case['corpus_digest'], 'operation' => $phase === 'digest' ? 'digest' : 'encode', 'input' => $input]);
                if (isset($result['finding'])) { return ['refusal' => $result['finding']]; }
                return $phase === 'digest' ? $result['sha256'] : $result['output'];
            }
            return $phase === 'digest' ? CanonicalDefinitionJson::checksum($input) : CanonicalDefinitionJson::encode($input);
        } catch (Kumwe\App\BusinessDefinition\Domain\InvalidBusinessDefinition $failure) {
            if (!$hostile) { throw $failure; }
            return ['refusal' => 'canonical.invalid-utf8'];
        }
    }
    if ($profile === 'decimal') {
        $left = $hostile ? str_repeat('9', 40) : '25000.00'; $right = $hostile ? str_repeat('9', 30) : '0.04938240';
        $p = $hostile ? 40 : 12; $s = $hostile ? 0 : 2; $rp = $hostile ? 30 : 12; $rs = $hostile ? 0 : 8;
        try {
            if ($native) {
                $input = 'KED1' . pack('VVVV', $size, 1048576, 1000000000, 0);
                for ($i = 0; $i < $size; ++$i) { $input .= pack('CCCCCCvv', 2, $p, $s, $rp, $rs, 0, strlen($left), strlen($right)) . $left . $right; }
                $result = $runtime->execute(['wire_version' => 1, 'profile' => $case['native_profile'],
                    'corpus_digest' => $case['corpus_digest'], 'input' => $input])['result'];
                if (substr($result, 0, 4) !== 'KER1' || unpack('V', substr($result, 4, 4))[1] !== $size) { throw new RuntimeException('Bad decimal boundary result.'); }
                $values = []; $offset = 8;
                for ($i = 0; $i < $size; ++$i) {
                    $length = unpack('V', substr($result, $offset, 4))[1]; $offset += 4;
                    $values[] = substr($result, $offset, $length); $offset += $length;
                }
                if ($offset !== strlen($result)) { throw new RuntimeException('Trailing decimal result.'); }
                return $values;
            }
            $values = [];
            for ($i = 0; $i < $size; ++$i) { $values[] = ExactDecimalArithmetic::multiply(ExactDecimal::fromString($left, $p, $s), ExactDecimal::fromString($right, $rp, $rs))->value(); }
            return $values;
        } catch (InvalidArgumentException $failure) { if (!$hostile) { throw $failure; } return ['refusal' => 1]; }
        catch (Kumwe\Engine\Exception\BindingFailure $failure) { if (!$hostile || $failure->getCode() !== 1) { throw $failure; } return ['refusal' => 1]; }
    }
    $documents = []; $php_results = [];
    if ($profile === 'report') {
        $rows = [];
        for ($i = 0; $i < $size; ++$i) { $rows[] = ['group' => 'group_' . ($i % 32), 'amount' => (($i % 10000) - 5000) . '.125']; }
        if ($hostile) { unset($rows[$size - 1]['amount']); }
        if ($native) { $documents[] = ['correlation' => 'report', 'input' => json_encode(['fields' => ['rows' => $rows], 'lines' => new stdClass()], JSON_FLAGS)]; }
        else {
            try { return [['rows' => $materialize->invoke($reportService, $case['php_plan'], $rows), 'findings' => []]]; }
            catch (Kumwe\App\BusinessReporting\Application\ReportUnavailable $failure) { if (!$hostile) { throw $failure; } return ['refusal' => 1]; }
        }
    } else {
        for ($i = 0; $i < $size; ++$i) {
            if ($profile === 'formula') { $values = ['amount' => $hostile ? PHP_INT_MAX : $i]; }
            else {
                $values = ['id' => ID];
                for ($n = 0; $n < $case['width']; ++$n) { $values['name_' . $n] = $hostile && $n === $case['width'] - 1 ? '' : 'record ' . $i; }
            }
            if ($native) {
                if ($profile === 'preparation') {
                    $entries = [];
                    foreach ($case['php_plan']->fields() as $field) {
                        if ($field->type === 'core.uuid' || !array_key_exists($field->handle, $values)) { continue; }
                        $raw = $values[$field->handle];
                        try { $normalized = ['value' => $codec->normalize($field, $raw, 'default', ID, ID), 'valid' => true]; }
                        catch (InvalidArgumentException) { $normalized = ['value' => null, 'valid' => false]; }
                        $entries[] = ['handle' => $field->handle, 'submitted' => $raw, 'normalized' => $normalized];
                    }
                    $values = ['operation' => 'create', 'current' => new stdClass(), 'input' => $entries, 'identity' => ID, 'allocated' => new stdClass()];
                }
                $documents[] = ['correlation' => 'row_' . $i, 'input' => json_encode(['fields' => $values, 'lines' => null], JSON_FLAGS)];
            } elseif ($profile === 'formula') {
                try { $php_results[] = ['value' => $case['php_plan']->evaluate($values), 'findings' => []]; }
                catch (InvalidArgumentException $failure) { if (!$hostile) { throw $failure; } return ['refusal' => 1]; }
            } elseif ($profile === 'preparation') {
                unset($values['id']);
                try { $prepared = $rules->create($case['php_plan'], $values, 'default', ID, ID, [], null); $php_results[] = ['values' => RecordValueGuard::canonical($prepared), 'findings' => []]; }
                catch (BusinessRecordValidationFailed $failure) { $php_results[] = ['findings' => findings($failure->violations)]; }
            } else {
                $violations = [];
                $compute->invokeArgs($rules, [$case['php_plan'], 'default', ID, &$values, &$violations]);
                $validate->invokeArgs($rules, [$case['php_plan'], &$values, &$violations, null]);
                $php_results[] = ['values' => RecordValueGuard::canonical($values), 'findings' => findings($violations)];
            }
        }
    }
    if (!$native) { return $php_results; }
    $out = [];
    foreach (array_chunk($documents, 64) as $chunk) {
    try {
        $request = ['plan_id' => $case['native_plan']['plan_id'], 'batch' => [
            'wire_version' => 1, 'documents' => $chunk, 'limits' => ['max_input_bytes' => 67108864,
                'max_output_bytes' => 16777216, 'max_documents' => 4096, 'max_findings' => 65536,
                'max_instructions' => 1000000000, 'max_milliseconds' => 30000]]];
        // Current Computation consumes only the Engine-owned JSON payload.
        // Old comparison modules retain their original mandatory decoded copy.
        if ($opaqueResults) { $request['result_format'] = 'opaque'; }
        $result = $case['runtime']->execute($request);
    } catch (Kumwe\Engine\Exception\BindingFailure $failure) {
        if (!in_array($profile, ['formula', 'report'], true) || !$hostile || $failure->getCode() !== 1) { throw $failure; }
        return ['refusal' => 1];
    }
    foreach ($result['results'] as $row) {
        $item = json_decode($row['result_json'], true, 512, JSON_THROW_ON_ERROR);
        if ($profile === 'preparation' && $item['findings'] !== []) { unset($item['values']); }
        $out[] = $item;
    }
    }
    return $out;
}
$sourcePaths = ['BusinessDefinition/Domain/Expression.php', 'BusinessDefinition/Domain/CanonicalDefinitionJson.php',
    'BusinessRecord/Application/RecordRuleValidator.php', 'BusinessRecord/Application/RecordValueCodec.php',
    'BusinessReporting/Application/ReportService.php'];
$hashes = [];
foreach ($sourcePaths as $path) { $hashes[$path] = hash_file('sha256', $config['app'] . '/src/' . $path); }
$observedCapabilities = $native ? $runtime->capabilities() : null;
$opaqueResults = $native && in_array('opaque-compiled-results/1', $observedCapabilities['binding_features'] ?? [], true);
$ready = ['ready' => true, 'backend' => $config['backend'], 'php' => PHP_VERSION, 'php_binary_sha256' => hash_file('sha256', PHP_BINARY), 'icu' => INTL_ICU_VERSION,
    'sources' => $hashes, 'conversion_reference' => Composer\InstalledVersions::getReference('kumwe/conversion'),
    'capabilities' => $observedCapabilities, 'compiled_result_format' => $native ? ($opaqueResults ? 'opaque' : 'both') : null,
    'zend_allocator' => getenv('USE_ZEND_ALLOC') !== '0'] + rss();
echo json_encode($ready, JSON_FLAGS) . "\n";
while (($line = fgets(STDIN)) !== false) {
    try {
        $job = json_decode($line, true, 512, JSON_THROW_ON_ERROR);
        if (($job['command'] ?? null) === 'stop') { break; }
        if (!is_int($job['iterations']) || $job['iterations'] < 1 || $job['iterations'] > 10000
            || !in_array($job['phase'], ['warm', 'cold', 'encode', 'digest'], true)
            || !in_array($job['shape'], ['valid', 'hostile'], true)) { throw new RuntimeException('Invalid job.'); }
        $key = json_encode([$job['profile'], $job['size'], $job['shape']]);
        $data = $cache[$key]['data'] ??= setup($job);
        $case = $cache[$key]['case'] ??= compile_case($data);
        $durations = []; $compileDurations = []; $hash = null; $outputBytes = 0;
        for ($n = 0; $n < $job['iterations']; ++$n) {
            $begin = hrtime(true);
            if ($job['phase'] === 'cold' && isset($data['program'])) {
                $temporary = compile_case($data); $compileDurations[] = hrtime(true) - $begin;
                $output = run_case($temporary, $job['phase']); unset($temporary);
            } else { $output = run_case($case, $job['phase']); }
            // Include the caller's final output serialization in both measured paths.
            $encoded = json_encode($output, JSON_FLAGS);
            $elapsed = hrtime(true) - $begin;
            $candidate = hash('sha256', json_encode(sorted($output), JSON_FLAGS));
            if ($hash !== null && $hash !== $candidate) { throw new RuntimeException('Nondeterministic benchmark output.'); }
            $hash = $candidate; $outputBytes = strlen($encoded); $durations[] = $elapsed;
        }
        echo json_encode(['ok' => true, 'digest' => $hash, 'durations_ns' => $durations,
            'output_bytes' => $outputBytes, 'compile_durations_ns' => $compileDurations,
            'dataset_descriptor_sha256' => hash('sha256', json_encode($data, JSON_FLAGS)), 'definition_fields' => $data['width'],
            'zend_used_bytes' => memory_get_usage(false), 'zend_peak_bytes' => memory_get_peak_usage(false)] + rss(), JSON_FLAGS) . "\n";
    } catch (Throwable $failure) {
        echo json_encode(['ok' => false, 'error_class' => $failure::class, 'error' => $failure->getMessage()], JSON_FLAGS) . "\n";
    }
}
