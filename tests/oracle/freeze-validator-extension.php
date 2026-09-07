<?php
declare(strict_types=1);
$app=realpath($argv[1]??'');$sdk=realpath($argv[2]??'');$autoload=realpath($argv[3]??'');$output=$argv[4]??null;
if($app===false||$sdk===false||$autoload===false||!is_string($output)){
    throw new RuntimeException('Usage: php freeze-validator-extension.php APP_BASELINE_ROOT SDK_BASELINE_ROOT COMPOSER_AUTOLOAD OUTPUT_JSON');
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
use Kumwe\App\BusinessRecord\Application\{RecordRuleValidator,RecordValueCodec,RecordExpressionValues};
use Kumwe\App\BusinessRecord\Infrastructure\Security\SodiumSecretCipher;
$rules=new RecordRuleValidator(new RecordValueCodec(new SodiumSecretCipher('test-corpus',str_repeat('x',32))));
$compute=new ReflectionMethod($rules,'compute');$validate=new ReflectionMethod($rules,'validate');
$base=['id'=>'018f4f24-98d8-7ad4-8f3f-38c909178b6b','owner'=>['type'=>'site','identifier'=>'default'],'site'=>'default','handle'=>'site.default.record','singular_label'=>'Record','plural_label'=>'Records','status'=>'draft','definition_version'=>0,'storage_mode'=>'relational','identity_strategy'=>'uuid','scope'=>'site','audit_enabled'=>true,'revisions_enabled'=>true,'fields'=>[],'relationships'=>[],'views'=>[],'actions'=>[],'workflow'=>null,'compatibility_metadata'=>[]];
$fixtures=[];
$add=static function(string $id,array $fields,array $input,array $invariants=[],?array $lines=null) use(&$fixtures,$base,$rules,$compute,$validate): void {
    $document=$base;$document['fields']=[['handle'=>'id','label'=>'ID','type'=>'core.uuid','required'=>true,'nullable'=>false,'read_only'=>true],...$fields];$input=['id'=>'018f4f24-98d8-7ad4-8f3f-38c909178b6b',...$input];$document['record_invariants']=$invariants;if($invariants!==[]){$document['relationships']=[['handle'=>'lines','label'=>'Lines','kind'=>'owned_line_collection','target'=>'site.default.record_line','ordered'=>true,'on_delete'=>'cascade']];}
    $definition=EntityTypeDefinition::fromArray($document);$values=$input;$violations=[];
    $args=[$definition,'default','018f4f24-98d8-7ad4-8f3f-38c909178b6b',&$values,&$violations,true];$compute->invokeArgs($rules,$args);
    $args=[$definition,$values,&$violations,$lines];$validate->invokeArgs($rules,$args);
    $fixtures[]=['id'=>$id,'definition'=>$definition->toArray(),'normalized_values'=>array_map(static fn($value)=>$value instanceof \Kumwe\Conversion\Decimal\ExactDecimal ? ['type'=>'exact-decimal','value'=>$value->value()] : $value,$input),'owned_lines'=>$lines,'expected'=>['values'=>RecordExpressionValues::from($values),'findings'=>array_map(static fn($v)=>['field'=>$v->field,'code'=>$v->code],$violations)]];
};
$field=static fn(string $handle,array $extra=[])=>array_merge(['handle'=>$handle,'label'=>$handle,'type'=>'core.integer','nullable'=>true],$extra);
$formula=static fn(string $handle,array $expression)=>$field($handle,['type'=>'core.computed','computed'=>true,'read_only'=>true,'server_only'=>true,'computation_mode'=>'stored','formula'=>$expression]);
$ref=static fn(string $handle)=>['op'=>'field','type'=>'integer','field'=>$handle];$lit=static fn(int $value)=>['op'=>'literal','type'=>'integer','value'=>$value];

$text=static fn(string $handle,array $validators)=>$field($handle,['type'=>'core.text','validators'=>$validators]);
$cases=[
 ['text_length_unicode','é🙂',[['rule'=>'min_length','value'=>3],['rule'=>'max_length','value'=>1]]],
 ['pattern_match','ABC-42',[['rule'=>'pattern','value'=>'^[A-Z]+-[0-9]+$']]],
 ['pattern_mismatch','prefix ABC-42 suffix',[['rule'=>'pattern','value'=>'^[A-Z]+-[0-9]+$']]],
 ['pattern_unanchored','prefix ABC-42 suffix',[['rule'=>'pattern','value'=>'[A-Z]+-[0-9]+']]],
 ['pattern_literal_delimiter','a~b',[['rule'=>'pattern','value'=>'^a~b$']]],
 ['pattern_invalid','text',[['rule'=>'pattern','value'=>'[']]],
 ['email_valid','a+b@example.com',[['rule'=>'email']]],
 ['email_missing_domain_dot','a@localhost',[['rule'=>'email']]],
 ['email_quoted_local','"a b"@example.com',[['rule'=>'email']]],
 ['email_unicode_local','é@example.com',[['rule'=>'email']]],
 ['url_https','https://example.com/path?a=1',[['rule'=>'url']]],
 ['url_ftp','ftp://example.com/file',[['rule'=>'url']]],
 ['url_missing_scheme','example.com/path',[['rule'=>'url']]],
 ['uuid_uppercase','018F4F24-98D8-7AD4-8F3F-38C909178B6B',[['rule'=>'uuid']]],
 ['uuid_braces','{018f4f24-98d8-7ad4-8f3f-38c909178b6b}',[['rule'=>'uuid']]],
 ['uuid_invalid','not-a-uuid',[['rule'=>'uuid']]],
 ['invalid_validator_bound','text',[['rule'=>'min_length','value'=>'2']]],
 ['strict_one_of','2',[['rule'=>'one_of','value'=>[2]]]],
];
foreach($cases as [$id,$value,$validators]){$add($id,[$text('value',$validators)],['value'=>$value]);}

$decimal=static fn(array $validators)=>$field('value',['type'=>'core.decimal','precision'=>5,'scale'=>2,'validators'=>$validators]);
$exact=\Kumwe\Conversion\Decimal\ExactDecimal::fromString('12.30',5,2);
$add('decimal_tag_pass',[$decimal([['rule'=>'decimal']])],['value'=>$exact]);
$add('decimal_string_refused',[$decimal([['rule'=>'decimal']])],['value'=>'12.30']);
$add('decimal_min_scale_normalization',[$decimal([['rule'=>'min','value'=>'12.3']])],['value'=>$exact]);
$add('decimal_max_fails',[$decimal([['rule'=>'max','value'=>'12.29']])],['value'=>$exact]);
$add('decimal_bound_precision_overflow',[$decimal([['rule'=>'min','value'=>'1000.00']])],['value'=>$exact]);
$add('decimal_bound_scale_overflow',[$decimal([['rule'=>'max','value'=>'12.301']])],['value'=>$exact]);
$add('decimal_one_of_canonical',[$decimal([['rule'=>'one_of','value'=>['12.30']]])],['value'=>$exact]);
$add('decimal_one_of_noncanonical',[$decimal([['rule'=>'one_of','value'=>['12.3']]])],['value'=>$exact]);
$path=$output;@mkdir(dirname($path),0777,true);
file_put_contents($path,json_encode(['schema'=>'kumwe-document-validation-corpus/v1','source_app'=>'24ecf956423c18933e824b43cea1bfb9127a79a9','oracle'=>'RecordRuleValidator::compute + validate (unchanged source; normalized scalar input; host RecordValueCodec only normalizes computed scalar outputs)','fixtures'=>$fixtures],JSON_PRETTY_PRINT|JSON_UNESCAPED_SLASHES|JSON_THROW_ON_ERROR)."\n");echo count($fixtures)." normalized document vectors frozen.\n";
