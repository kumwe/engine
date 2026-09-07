<?php
declare(strict_types=1);
$app=realpath($argv[1]??'');$sdk=realpath($argv[2]??'');$autoload=realpath($argv[3]??'');$output=$argv[4]??null;
if($app===false||$sdk===false||$autoload===false||!is_string($output)){
    throw new RuntimeException('Usage: php freeze-validator-edges.php APP_BASELINE_ROOT SDK_BASELINE_ROOT COMPOSER_AUTOLOAD OUTPUT_JSON');
}
$sourceFiles=[
    'src/BusinessRecord/Application/RecordRuleValidator.php'=>'329c5134f47d12b6406efabc600b6b5bbe6d5785b100182f9bded4437cf28a13',
    'src/BusinessRecord/Application/RecordValueCodec.php'=>'7028ac009854b53bd7af010c0583d8c4ecefec647bd7bd7b21762232fa453a72',
    'src/BusinessRecord/Application/RecordExpressionValues.php'=>'c4b6e749729fc0e28953f43db454554f613091b21ab5ab96f341201a9eea7fa1',
    'src/BusinessRecord/Domain/RecordValueGuard.php'=>'3fa9cc301900cac3b84f2a5b3b37b81824521cad927ab22be6aac6d27a94b028',
    'src/BusinessRecord/Domain/EncryptedEnvelope.php'=>'61f1131a786e82c5a4c19c1d53626c89af52c6f8139d3de9ef0a23c6d8b177fb',
    'src/BusinessDefinition/Domain/EntityTypeDefinition.php'=>'cbcd1856e5562160112d08a34713de45b25215bb36b0165d83f8bc8184fc92d9',
    'src/BusinessDefinition/Domain/FieldDefinition.php'=>'c6b37e65201a4642a5c3e3aa09b05aec76db2c1bbab78464bf36d936f9d80d5b',
];
foreach($sourceFiles as $file=>$digest){
    if(hash_file('sha256',$app.'/'.$file)!==$digest)throw new RuntimeException('App oracle source mismatch: '.$file);
}
if(hash_file('sha256',$sdk.'/src/Spi/BusinessRecord/Value/ZonedDateTimeValue.php')
    !=='6d3bb1abb46b66cda07c807120598cf05a1dfee5c0b5cab37d53edde8ab50ff2'){
    throw new RuntimeException('SDK oracle zoned value source mismatch.');
}
require $autoload;
spl_autoload_register(static function(string $name) use($app,$sdk):void {
    foreach(['Kumwe\\App\\'=>$app.'/src/','Kumwe\\Extension\\'=>$sdk.'/src/'] as $prefix=>$root){
        if(str_starts_with($name,$prefix)){
            $file=$root.str_replace('\\','/',substr($name,strlen($prefix))).'.php';
            if(is_file($file))require $file;
            return;
        }
    }
});
use Kumwe\App\BusinessDefinition\Domain\EntityTypeDefinition;
use Kumwe\App\BusinessRecord\Application\RecordExpressionValues;
use Kumwe\App\BusinessRecord\Application\RecordRuleValidator;
use Kumwe\App\BusinessRecord\Application\RecordValueCodec;
use Kumwe\App\BusinessRecord\Infrastructure\Security\SodiumSecretCipher;
use Kumwe\Conversion\Decimal\ExactDecimal;

$rules = new RecordRuleValidator(new RecordValueCodec(new SodiumSecretCipher('edge-corpus', str_repeat('x', 32))));
$compute = new ReflectionMethod($rules, 'compute');
$validate = new ReflectionMethod($rules, 'validate');
$base = [
    'id' => '018f4f24-98d8-7ad4-8f3f-38c909178b6b',
    'owner' => ['type' => 'site', 'identifier' => 'default'],
    'site' => 'default', 'handle' => 'site.default.record',
    'singular_label' => 'Record', 'plural_label' => 'Records',
    'status' => 'draft', 'definition_version' => 0, 'storage_mode' => 'relational',
    'identity_strategy' => 'uuid', 'scope' => 'site', 'audit_enabled' => true,
    'revisions_enabled' => true, 'fields' => [], 'relationships' => [],
    'views' => [], 'actions' => [], 'workflow' => null, 'compatibility_metadata' => [],
];
$fixtures = [];
$warnings = [];
$activeId = '';
// Preserve preg_match's false result while collecting, instead of printing,
// the warnings its deliberately invalid patterns produce.
set_error_handler(static function (int $severity, string $message) use (&$warnings, &$activeId): bool {
    if ($severity !== E_WARNING || !str_starts_with($message, 'preg_match():')) {
        return false;
    }
    $warnings[$activeId][] = $message;
    return true;
});
$add = static function (string $id, mixed $value, array $validators, array $extra = [], bool $absent = false) use (
    &$fixtures, &$activeId, $base, $rules, $compute, $validate,
): void {
    $activeId = $id;
    $document = $base;
    $document['fields'] = [
        ['handle' => 'id', 'label' => 'ID', 'type' => 'core.uuid', 'required' => true, 'nullable' => false, 'read_only' => true],
        array_merge(['handle' => 'value', 'label' => 'Value', 'type' => 'core.text', 'nullable' => true, 'validators' => $validators], $extra),
    ];
    $input = ['id' => $base['id']];
    if (!$absent) {
        $input['value'] = $value;
    }
    $definition = EntityTypeDefinition::fromArray($document);
    $values = $input;
    $violations = [];
    $arguments = [$definition, 'default', $base['id'], &$values, &$violations, true];
    $compute->invokeArgs($rules, $arguments);
    $arguments = [$definition, $values, &$violations, null];
    $validate->invokeArgs($rules, $arguments);
    $fixtures[] = [
        'id' => $id,
        'definition' => $definition->toArray(),
        'normalized_values' => array_map(static fn ($candidate) => $candidate instanceof ExactDecimal
            ? ['type' => 'exact-decimal', 'value' => $candidate->value()] : $candidate, $input),
        'owned_lines' => null,
        'expected' => [
            'values' => RecordExpressionValues::from($values),
            'findings' => array_map(static fn ($violation) => ['field' => $violation->field, 'code' => $violation->code], $violations),
        ],
    ];
};
$urls = [
    'url_scheme_case' => 'HtTpS://Example.COM/',
    'url_single_label' => 'http://localhost/',
    'url_numeric_nondotted_host' => 'http://2130706433/',
    'url_non_ip_numeric_host' => 'http://999.999.999.999/',
    'url_trailing_dot' => 'https://example.com./',
    'url_trailing_hyphen' => 'https://example.com-/',
    'url_trailing_hyphen_dot' => 'https://example.com-./',
    'url_leading_hyphen' => 'https://-example.com/',
    'url_interior_empty_label' => 'https://example..com/',
    'url_underscore_label' => 'https://under_score.example/',
    'url_punycode_host' => 'https://xn--bcher-kva.example/',
    'url_unicode_host' => 'https://bücher.example/',
    'url_label_63' => 'https://' . str_repeat('a', 63) . '.example/',
    'url_label_64' => 'https://' . str_repeat('a', 64) . '.example/',
    'url_host_253' => 'https://' . implode('.', [str_repeat('a', 63), str_repeat('b', 63), str_repeat('c', 63), str_repeat('d', 61)]) . '/',
    'url_host_254_trailing_dot' => 'https://' . implode('.', [str_repeat('a', 63), str_repeat('b', 63), str_repeat('c', 63), str_repeat('d', 61)]) . './',
    'url_empty_port' => 'http://example.com:/',
    'url_port_zero' => 'http://example.com:0/',
    'url_port_maximum' => 'http://example.com:65535/',
    'url_port_overflow' => 'http://example.com:65536/',
    'url_port_positive_sign' => 'http://example.com:+80/',
    'url_port_negative_zero' => 'http://example.com:-0/',
    'url_port_negative_one' => 'http://example.com:-1/',
    'url_port_suffix_ascii' => 'http://example.com:80abc/',
    'url_port_suffix_punctuation' => 'http://example.com:8!/',
    'url_port_six_digits' => 'http://example.com:000080/',
    'url_port_double_colon' => 'http://example.com:80:90/',
    'url_empty_user' => 'http://@example.com/',
    'url_user_password' => 'http://user:password@example.com/',
    'url_empty_user_password' => 'http://:password@example.com/',
    'url_user_multiple_colons' => 'http://user:pass:word@example.com/',
    'url_user_multiple_at' => 'http://user@name@example.com/',
    'url_user_percent_numeric' => 'http://u%2Fser@example.com/',
    'url_user_percent_alpha' => 'http://u%AFser@example.com/',
    'url_user_percent_invalid' => 'http://u%2Gser@example.com/',
    'url_user_safe_punctuation' => "http://a-._~!$&'()*+,;=:b@example.com/",
    'url_ipv6_loopback' => 'http://[::1]/',
    'url_ipv6_full' => 'http://[2001:0db8:0000:0000:0000:0000:0000:0001]/',
    'url_ipv6_port' => 'http://[2001:db8::1]:443/',
    'url_ipv6_embedded_ipv4' => 'http://[::ffff:192.0.2.1]/',
    'url_ipv6_embedded_ipv4_leading_zero' => 'http://[::ffff:192.00.2.1]/',
    'url_ipv6_zone' => 'http://[fe80::1%25eth0]/',
    'url_ipv6_multiple_compression' => 'http://[2001::db8::1]/',
    'url_ipv6_compression_no_room' => 'http://[1:2:3:4:5:6:7:8::]/',
    'url_ipv6_missing_brackets' => 'http://2001:db8::1/',
    'url_ipv6_empty' => 'http://[]/',
    'url_path_unusual_ascii' => 'https://example.com/{}|\\^`<>"[]',
    'url_path_invalid_percent' => 'https://example.com/%xx',
    'url_path_unicode' => 'https://example.com/é',
    'url_path_space' => 'https://example.com/a b',
    'url_path_control' => "https://example.com/a\tb",
];
foreach ($urls as $id => $value) {
    $add($id, $value, [['rule' => 'url']]);
}
$emails = [
    'email_ascii_atom_symbols' => "!#$%&'*+-/=?^_`{|}~@example.com",
    'email_quoted_escaped_space' => '"a\\ b"@example.com',
    'email_quoted_empty' => '""@example.com',
    'email_dotted_quotes' => '"a"."b"@example.com',
    'email_double_dot_local' => 'a..b@example.com',
    'email_local_64' => str_repeat('a', 64) . '@example.com',
    'email_local_65' => str_repeat('a', 65) . '@example.com',
    'email_total_254' => str_repeat('a', 64) . '@' . implode('.', [str_repeat('b', 63), str_repeat('c', 63), str_repeat('d', 61)]),
    'email_total_255' => str_repeat('a', 64) . '@' . implode('.', [str_repeat('b', 63), str_repeat('c', 63), str_repeat('d', 62)]),
    'email_ipv4_literal' => 'a@[192.0.2.1]',
    'email_ipv4_invalid_literal' => 'a@[256.0.2.1]',
    'email_ipv6_literal' => 'a@[IPv6:2001:db8::1]',
    'email_ipv6_label_case' => 'a@[ipv6:::1]',
    'email_ipv6_mapped' => 'a@[IPv6:::ffff:192.0.2.1]',
    'email_domain_numeric_tld' => 'a@example.123',
    'email_domain_punycode_tld' => 'a@example.xn--p1ai',
    'email_domain_unicode' => 'a@bücher.example',
    'email_trailing_dot' => 'a@example.com.',
    'email_trailing_newline' => "a@example.com\n",
];
foreach ($emails as $id => $value) {
    $add($id, $value, [['rule' => 'email']]);
}
$uuid = '018f4f24-98d8-7ad4-8f3f-38c909178b6b';
foreach ([
    'uuid_nil' => '00000000-0000-0000-0000-000000000000',
    'uuid_all_hex_unassigned_version' => 'ffffffff-ffff-ffff-ffff-ffffffffffff',
    'uuid_urn' => 'urn:uuid:' . $uuid,
    'uuid_uppercase_urn' => 'URN:UUID:' . $uuid,
    'uuid_mixed_case_urn' => 'Urn:Uuid:' . $uuid,
    'uuid_repeated_prefix' => 'urn:urn:uuid:' . $uuid,
    'uuid_midstream_tokens' => '018f{4f24}-98d8-uuid:7ad4-8f3f-38c909178b6b',
    'uuid_unbalanced_brace' => '{' . $uuid,
    // PHP str_replace does not rescan a token formed by its own deletion.
    'uuid_prefix_token_reappearance' => 'ururn:n:' . $uuid,
    'uuid_without_hyphens' => str_replace('-', '', $uuid),
    'uuid_newline' => $uuid . "\n",
] as $id => $value) {
    $add($id, $value, [['rule' => 'uuid']]);
}
$patterns = [
    ['pattern_unicode_property', 'ΔΩ', '^\\p{Greek}+$'],
    ['pattern_named_backreference', 'abc-abc', '^(?<word>[a-z]+)-\\k<word>$'],
    ['pattern_fixed_lookbehind', 'abc', '(?<=ab)c'],
    ['pattern_variable_lookbehind', 'aab', '(?<=a{1,2})b'],
    ['pattern_dollar_final_newline', "text\n", '^text$'],
    ['pattern_inline_multiline', "text\nnext", '(?m)^text$'],
    ['pattern_inline_dotall', "a\nb", '(?s)^a.b$'],
    ['pattern_unicode_byte_escape', 'é', '^\\C\\C$'],
    ['pattern_delimiter_preescaped_odd', 'a~b', '^a\\~b$'],
    ['pattern_delimiter_preescaped_even', 'a\\~b', '^a\\\\~b$'],
    ['pattern_empty_alternative', 'anything', '(?:nope|)'],
    ['pattern_unclosed_group', 'a', '(a'],
    ['pattern_duplicate_named_group', 'aa', '(?<x>a)(?<x>a)'],
    ['pattern_length_512', str_repeat('a', 512), str_repeat('a', 512)],
    ['pattern_length_513', str_repeat('a', 513), str_repeat('a', 513)],
    ['pattern_inline_limit_not_at_start', 'a', '(*LIMIT_MATCH=5)a'],
    ['pattern_catastrophic_match_limit', str_repeat('a', 256) . '!', '^(a+)+$'],
    ['pattern_recursive_balanced_small', str_repeat('a', 40) . str_repeat('b', 40), '^(?<p>a(?&p)?b)$'],
    ['pattern_recursive_balanced_depth', str_repeat('a', 600) . str_repeat('b', 600), '^(?<p>a(?&p)?b)$'],
    ['pattern_recursive_balanced_stack', str_repeat('a', 2000) . str_repeat('b', 2000), '^(?<p>a(?&p)?b)$'],
];
foreach ($patterns as [$id, $value, $expression]) {
    $add($id, $value, [['rule' => 'pattern', 'value' => $expression]]);
}

// The value type deliberately exercises the App's && short circuit before
// malformed parameters are read. Null/absence skip all validators entirely.
foreach ([
    ['malformed_min_length_string', 'text', [['rule' => 'min_length']]],
    ['malformed_min_length_integer', 7, [['rule' => 'min_length']]],
    ['malformed_max_length_boolean', false, [['rule' => 'max_length', 'value' => ['bad']]]],
    ['malformed_pattern_string', 'text', [['rule' => 'pattern']]],
    ['malformed_pattern_integer', 7, [['rule' => 'pattern']]],
    ['malformed_pattern_boolean', false, [['rule' => 'pattern', 'value' => '[']]],
    ['malformed_pattern_empty_string', '', [['rule' => 'pattern', 'value' => '']]],
    ['malformed_rule_missing', 'text', [['value' => 'x']]],
    ['malformed_rule_nonstring', 'text', [['rule' => 1]]],
    ['malformed_rule_unknown', 'text', [['rule' => 'unregistered']]],
    ['malformed_one_of_empty', 'text', [['rule' => 'one_of', 'value' => []]]],
    ['malformed_one_of_later_invalid_entry', 'text', [['rule' => 'one_of', 'value' => ['text', ['bad']]]]],
    ['malformed_one_of_associative', 'text', [['rule' => 'one_of', 'value' => ['a' => 'text']]]],
    ['malformed_min_integer_bound_number', 7, [['rule' => 'min', 'value' => 2]]],
    ['malformed_min_integer_bound_plus', 7, [['rule' => 'min', 'value' => '+2']]] ,
    ['malformed_max_integer_bound_leading_zero', 7, [['rule' => 'max', 'value' => '08']]],
    ['range_integer_bound_negative_zero', 0, [['rule' => 'min', 'value' => '-0']]],
    ['range_string_numeric_comparison', '20', [['rule' => 'min', 'value' => '3']]],
    ['range_string_numeric_exponent', '1e3', [['rule' => 'max', 'value' => '900']]],
    ['range_string_trailing_whitespace', '9 ', [['rule' => 'min', 'value' => '10']]],
    ['rule_arguments_unused', 'a@example.com', [['rule' => 'email', 'value' => ['irrelevant']]]],
    ['malformed_multiple_findings_order', 7, [['rule' => 'pattern'], ['rule' => 'min_length'], ['value' => 1], ['rule' => 'uuid']]],
] as [$id, $value, $validators]) {
    $add($id, $value, $validators, ['type' => is_int($value) ? 'core.integer' : (is_bool($value) ? 'core.boolean' : 'core.text')]);
}
$invalidRules = [['rule' => 'pattern', 'value' => '['], ['rule' => 'unregistered'], ['rule' => 'min_length']];
$add('null_skips_invalid_rules', null, $invalidRules);
$add('absence_skips_invalid_rules', null, $invalidRules, [], true);
$add('required_skips_invalid_rules', null, $invalidRules, ['required' => true, 'nullable' => false]);
$add('nonnullable_skips_invalid_rules', null, $invalidRules, ['nullable' => false]);
$add('exact_decimal_shortcircuits_string_rules', ExactDecimal::fromString('12.30', 5, 2), [
    ['rule' => 'pattern'], ['rule' => 'min_length'], ['rule' => 'max_length', 'value' => []], ['rule' => 'url'],
], ['type' => 'core.decimal', 'precision' => 5, 'scale' => 2]);

restore_error_handler();
$source = $app . '/src/BusinessRecord/Application/RecordRuleValidator.php';
$corpus = [
    'schema' => 'kumwe-document-validation-corpus/v1',
    'source_app' => '24ecf956423c18933e824b43cea1bfb9127a79a9',
    'source_validator_sha256' => hash_file('sha256', $source),
    'oracle' => 'Unchanged App RecordRuleValidator::compute + validate; expected values/findings are frozen exclusively from PHP reflection.',
    'oracle_runtime' => ['php' => PHP_VERSION, 'pcre' => PCRE_VERSION, 'pcre_jit' => ini_get('pcre.jit')],
    'oracle_pattern_warnings' => $warnings,
    'fixtures' => $fixtures,
];
$path = $output;
file_put_contents($path, json_encode($corpus, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES | JSON_THROW_ON_ERROR) . "\n");
echo count($fixtures) . " focused document validator edge vectors frozen from PHP.\n";
