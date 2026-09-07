<?php
// Regenerate the independent PHP 8.5.10 / ICU 74.2 normalizer oracle. Not used by native execution.
declare(strict_types=1);
if (PHP_VERSION !== '8.5.10' || INTL_ICU_VERSION !== '74.2') {
    throw new RuntimeException('Unicode oracle needs exactly PHP 8.5.10 and ICU 74.2.');
}
$root = dirname(__DIR__, 2);
$contexts = array_fill_keys(range(0, 127), true);
foreach (file($root . '/third_party/unicode/17.0.0/DerivedCoreProperties.txt') as $line) {
    if (!preg_match('/^([0-9A-F]+)(?:\.\.([0-9A-F]+))?\s*;\s*(?:Cased|Case_Ignorable)\s*#/', $line, $match)) {
        continue;
    }
    $start = hexdec($match[1]);
    $end = ($match[2] ?? '') === '' ? $start : hexdec($match[2]);
    for ($cp = max(0, $start - 1); $cp <= min(0x10ffff, $end + 1); ++$cp) {
        $contexts[$cp] = true;
    }
}
ksort($contexts, SORT_NUMERIC);
$operations = ['lowercase', 'uppercase', 'unicode_nfc'];
$hashes = [];
foreach ($operations as $operation) { $hashes[$operation] = hash_init('sha256'); }
$contextHash = hash_init('sha256');
$feed = static function (HashContext $hash, string $value): void {
    hash_update($hash, pack('N', strlen($value)) . $value);
};
$count = 0;
for ($cp = 0; $cp <= 0x10ffff; ++$cp) {
    if ($cp >= 0xd800 && $cp <= 0xdfff) { continue; }
    $text = mb_chr($cp, 'UTF-8');
    $feed($hashes['lowercase'], mb_strtolower($text, 'UTF-8'));
    $feed($hashes['uppercase'], mb_strtoupper($text, 'UTF-8'));
    $feed($hashes['unicode_nfc'], Normalizer::normalize($text, Normalizer::FORM_C));
    if (isset($contexts[$cp])) {
        foreach ([$text . 'Σ', 'A' . $text . 'Σ', 'AΣ' . $text, 'AΣ' . $text . 'A'] as $probe) {
            $feed($contextHash, mb_strtolower($probe, 'UTF-8'));
        }
    }
    ++$count;
}
$output = ['php' => PHP_VERSION, 'icu' => INTL_ICU_VERSION, 'scalar_count' => $count,
    'context_points' => array_keys($contexts), 'context_sha256' => hash_final($contextHash), 'sha256' => []];
foreach ($operations as $operation) { $output['sha256'][$operation] = hash_final($hashes[$operation]); }
echo json_encode($output, JSON_PRETTY_PRINT | JSON_THROW_ON_ERROR), "\n";
