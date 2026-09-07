<?php
declare(strict_types=1);
$app=realpath($argv[1]??'');$sdk=realpath($argv[2]??'');$autoload=realpath($argv[3]??'');$output=$argv[4]??null;
if($app===false||$sdk===false||$autoload===false||!is_string($output)){
    throw new RuntimeException('Usage: php freeze-normalized-values.php APP_BASELINE_ROOT SDK_BASELINE_ROOT COMPOSER_AUTOLOAD OUTPUT_JSON');
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
$codec=new RecordValueCodec(new SodiumSecretCipher('test-corpus',str_repeat('x',32)));
$rules=new RecordRuleValidator($codec);
$compute=new ReflectionMethod($rules,'compute');$validate=new ReflectionMethod($rules,'validate');
$base=['id'=>'018f4f24-98d8-7ad4-8f3f-38c909178b6b','owner'=>['type'=>'site','identifier'=>'default'],'site'=>'default','handle'=>'site.default.record','singular_label'=>'Record','plural_label'=>'Records','status'=>'draft','definition_version'=>0,'storage_mode'=>'relational','identity_strategy'=>'uuid','scope'=>'site','audit_enabled'=>true,'revisions_enabled'=>true,'fields'=>[],'relationships'=>[],'views'=>[],'actions'=>[],'workflow'=>null,'compatibility_metadata'=>[]];

$fixtures=[];
$recordId='018f4f24-98d8-7ad4-8f3f-38c909178b6b';
$encode=static function($value) use(&$encode) {
    if($value instanceof \Kumwe\Conversion\Decimal\ExactDecimal)return ['type'=>'exact-decimal','value'=>$value->value()];
    $kind=match(true){
        $value instanceof \Kumwe\Conversion\Value\MoneyValue=>'money',
        $value instanceof \Kumwe\Conversion\Value\QuantityValue=>'quantity',
        $value instanceof \Kumwe\Extension\Spi\BusinessRecord\Value\ZonedDateTimeValue=>'zoned-datetime',
        $value instanceof \DateTimeImmutable=>'datetime',
        $value instanceof \Kumwe\App\BusinessRecord\Domain\EncryptedEnvelope=>'encrypted',
        is_array($value)=>'array',default=>null,
    };
    if($kind===null)return $value;
    if($kind==='array'){
        $entries=[];foreach($value as $key=>$item)$entries[]=['key'=>$key,'value'=>$encode($item)];
        $payload=['entries'=>$entries];
    }else $payload=\Kumwe\App\BusinessRecord\Domain\RecordValueGuard::canonical($value);
    return ['type'=>'normalized-value','version'=>1,'kind'=>$kind,'value'=>$payload];
};
$add=static function(string $id,array $fields,array $input,array $invariants=[],?array $lines=null) use(&$fixtures,$base,$rules,$compute,$validate,$encode): void {
    $document=$base;$document['fields']=[['handle'=>'id','label'=>'ID','type'=>'core.uuid','required'=>true,'nullable'=>false,'read_only'=>true],...$fields];$input=['id'=>'018f4f24-98d8-7ad4-8f3f-38c909178b6b',...$input];$document['record_invariants']=$invariants;if($invariants!==[]){$document['relationships']=[['handle'=>'lines','label'=>'Lines','kind'=>'owned_line_collection','target'=>'site.default.record_line','ordered'=>true,'on_delete'=>'cascade']];}
    $definition=EntityTypeDefinition::fromArray($document);$values=$input;$violations=[];
    $args=[$definition,'default','018f4f24-98d8-7ad4-8f3f-38c909178b6b',&$values,&$violations,true];$compute->invokeArgs($rules,$args);
    $args=[$definition,$values,&$violations,$lines];$validate->invokeArgs($rules,$args);
    $fixtures[]=['id'=>$id,'definition'=>$definition->toArray(),'normalized_values'=>array_map($encode,$input),'owned_lines'=>$lines,'expected'=>['values'=>\Kumwe\App\BusinessRecord\Domain\RecordValueGuard::canonical($values),'findings'=>array_map(static fn($v)=>['field'=>$v->field,'code'=>$v->code],$violations)]];
};
$field=static fn(string $handle,array $extra=[])=>array_merge(['handle'=>$handle,'label'=>$handle,'type'=>'core.integer','nullable'=>true],$extra);
$formula=static fn(string $handle,array $expression)=>$field($handle,['type'=>'core.computed','computed'=>true,'read_only'=>true,'server_only'=>true,'computation_mode'=>'stored','formula'=>$expression]);
$ref=static fn(string $handle)=>['op'=>'field','type'=>'integer','field'=>$handle];$lit=static fn(int $value)=>['op'=>'literal','type'=>'integer','value'=>$value];

$text=static fn(string $handle,array $validators)=>$field($handle,['type'=>'core.text','validators'=>$validators]);

$decimal=\Kumwe\Conversion\Decimal\ExactDecimal::fromString('12.30',5,2);
$kinds=[
 'money'=>new \Kumwe\Conversion\Value\MoneyValue($decimal,'USD'),
 'quantity'=>new \Kumwe\Conversion\Value\QuantityValue($decimal,'kg'),
 'datetime_extended_year'=>new \DateTimeImmutable('+10000-01-01T09:05:00Z'),
 'datetime_negative_year'=>new \DateTimeImmutable('-0001-01-01T09:05:00Z'),
 'datetime'=>new \DateTimeImmutable('2026-09-07T10:11:12.123456+02:00'),
 'zoned'=>\Kumwe\Extension\Spi\BusinessRecord\Value\ZonedDateTimeValue::fromStrings('2026-09-07T08:11:12.123456Z','Africa/Johannesburg'),
 'encrypted'=>new \Kumwe\App\BusinessRecord\Domain\EncryptedEnvelope('opaque-ciphertext',str_repeat('n',24),'test-key'),
 'list'=>[true,2,'x',null],
 'map'=>['z'=>2,'a'=>'first'],
 'nested'=>['z'=>[$decimal,new \DateTimeImmutable('2026-09-07T00:00:00+00:00')],'a'=>[1=>'one',0=>'zero']],
];
foreach($kinds as $name=>$value){
 $add($name.'_canonical',[$field('value',['type'=>'core.text','required'=>true,'nullable'=>false])],['value'=>$value]);
 $validators=[['rule'=>'min_length','value'=>0],['rule'=>'max_length','value'=>1000],['rule'=>'pattern','value'=>'.*'],['rule'=>'email'],['rule'=>'url'],['rule'=>'uuid'],['rule'=>'integer'],['rule'=>'decimal'],['rule'=>'min','value'=>'0'],['rule'=>'one_of','value'=>['2026-09-07T10:11:12.123456+02:00']]];
 $add($name.'_runtime_kind',[$field('value',['type'=>'core.text','validators'=>$validators])],['value'=>$value]);
 $expression=RecordExpressionValues::from(['value'=>$value])['value'];
 $invariant=['handle'=>'projection','message'=>'Projection must match','condition'=>$expression===null ? ['op'=>'is_null','type'=>'boolean','args'=>[['op'=>'field','type'=>'string','field'=>'value']]] : ['op'=>'eq','type'=>'boolean','args'=>[['op'=>'field','type'=>'string','field'=>'value'],['op'=>'literal','type'=>'string','value'=>$expression]]]];
 $add($name.'_expression_projection',[$field('value',['type'=>'core.text'])],['value'=>$value],[$invariant]);
}
file_put_contents($output,json_encode(['schema'=>'kumwe-document-validation-corpus/v1','source_app'=>'24ecf956423c18933e824b43cea1bfb9127a79a9','oracle'=>'Unchanged App RecordRuleValidator compute/validate with actual normalized PHP domain values; output is RecordValueGuard::canonical, expression projection is asserted by original invariants.','fixtures'=>$fixtures],JSON_PRETTY_PRINT|JSON_UNESCAPED_SLASHES|JSON_THROW_ON_ERROR)."\n");
echo count($fixtures)." normalized runtime-kind vectors frozen.\n";
