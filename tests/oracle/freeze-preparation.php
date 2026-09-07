<?php
declare(strict_types=1);
$app=realpath($argv[1]??'');$sdk=realpath($argv[2]??'');$autoload=realpath($argv[3]??'');$output=$argv[4]??null;
if($app===false||$sdk===false||$autoload===false||!is_string($output)){
    throw new RuntimeException('Usage: php freeze-preparation.php APP_BASELINE_ROOT SDK_BASELINE_ROOT COMPOSER_AUTOLOAD OUTPUT_JSON');
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
$instances=new \SplObjectStorage();
$encode=static function($value) use(&$encode,&$instances) {
    if(is_object($value)&&!$instances->offsetExists($value))$instances[$value]=count($instances)+1;
    if($value instanceof \Kumwe\Conversion\Decimal\ExactDecimal){
        return ['type'=>'exact-decimal','value'=>$value->value(),'instance'=>$instances[$value]];
    }
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
    $tag=['type'=>'normalized-value','version'=>1,'kind'=>$kind,'value'=>$payload];
    if(is_object($value))$tag['instance']=$instances[$value];
    return $tag;
};
$normalize=static function($field,$value) use($codec,$recordId,$encode): array {
    try { return ['value'=>$encode($codec->normalize($field,$value,'default','018f4f24-98d8-7ad4-8f3f-38c909178b6b',$recordId)),'valid'=>true]; }
    catch(InvalidArgumentException){return ['value'=>null,'valid'=>false];}
};
$withoutInstances=static function($value) use(&$withoutInstances){
    if(!is_array($value))return $value;
    unset($value['instance']);
    return array_map($withoutInstances,$value);
};
$add=static function(string $id,string $mode,array $declarations,array $input,array $current=[],array $allocated=[])
    use(&$fixtures,&$instances,$base,$rules,$codec,$normalize,$withoutInstances,$encode,$recordId):void {
    $instances=new \SplObjectStorage();
    $document=$base;
    $document['fields']=[['handle'=>'id','label'=>'ID','type'=>'core.uuid','required'=>true,'nullable'=>false,'read_only'=>true],...$declarations];
    $definition=EntityTypeDefinition::fromArray($document);
    $fields=[];$validation=[];$lookup=[];
    foreach($definition->fields() as $field){
        $lookup[$field->handle]=$field;
        $fields[]=['handle'=>$field->handle,'identity'=>in_array($field->type,['core.uuid','core.reference_identity'],true),
            'sequence'=>$field->type==='core.sequence','computed'=>$field->computed||$field->formula!==null,
            'server_only'=>$field->serverOnly,'read_only'=>$field->readOnly,'immutable_after_create'=>$field->immutableAfterCreate,
            'default'=>$withoutInstances($normalize($field,$field->default)),
            'visibility_condition'=>$field->visibilityCondition?->toArray(),
            'editability_condition'=>$field->editabilityCondition?->toArray()];
        $validation[]=['handle'=>$field->handle,'required'=>$field->required,'nullable'=>$field->nullable,
            'formula'=>$field->formula?->toArray(),'validators'=>$field->validators,'type'=>$field->type,
            'precision'=>$field->precision,'scale'=>$field->scale,'length'=>$field->length,'normalizers'=>$field->normalizers];
    }
    $entries=[];
    foreach($input as $handle=>$raw)$entries[]=['handle'=>$handle,'submitted'=>$encode($raw),
        'normalized'=>isset($lookup[$handle])?$normalize($lookup[$handle],$raw):['value'=>null,'valid'=>true]];
    $numbers=[];foreach($allocated as $handle=>$number)$numbers[$handle]=$normalize($lookup[$handle],$number);
    if($mode==='update')$current=['id'=>$recordId,...$current];
    $values=null;$findings=[];
    try {
        $values=$mode==='create'
            ?$rules->create($definition,$input,'default',$recordId,$recordId,$allocated,null)
            :$rules->update($definition,$current,$input,'default',$recordId,$recordId,null);
    } catch(\Kumwe\App\BusinessRecord\Application\Exception\BusinessRecordValidationFailed $failure){
        $findings=array_map(static fn($v)=>['field'=>$v->field,'code'=>$v->code],$failure->violations);
    }
    $fixtures[]=['id'=>$id,'definition'=>$definition->toArray(),
        'program'=>['fields'=>$fields,'validation'=>['fields'=>$validation,'invariants'=>[]]],
        'input'=>['operation'=>$mode,'current'=>$current===[]?(object)[]:array_map($encode,$current),'input'=>$entries,
            'identity'=>$recordId,'allocated'=>$numbers===[]?(object)[]:$numbers],
        'lines'=>null,'expected'=>['returns_values'=>$values!==null,
            'values'=>$values===null?null:\Kumwe\App\BusinessRecord\Domain\RecordValueGuard::canonical($values),
            'findings'=>$findings]];
};
$f=static fn(string $handle,array $extra=[])=>array_merge(['handle'=>$handle,'label'=>$handle,'type'=>'core.text','nullable'=>true],$extra);
$literal=static fn($value,string $type)=>['op'=>'literal','type'=>$type,'value'=>$value];
$ref=static fn(string $handle,string $type)=>['op'=>'field','type'=>$type,'field'=>$handle];
$computed=static fn(string $handle,array $formula)=>$f($handle,['type'=>'core.computed','computed'=>true,'read_only'=>true,'server_only'=>true,'computation_mode'=>'stored','formula'=>$formula]);
$add('create_defaults_and_codec','create',[$f('name',['default'=>'  DEFAULT  ','normalizers'=>['trim','lowercase']]),$f('server',['server_only'=>true,'default'=>'system'])],[]);
$add('create_identity_uses_host_value','create',[],['id'=>'caller-spelling']);
$add('create_unknown_input_order','create',[$f('name')],['z_unknown'=>1,'a_unknown'=>2,'name'=>'ok']);
$add('create_readonly_computed','create',[$f('readonly',['read_only'=>true]),$computed('total',$literal(7,'integer'))],['readonly'=>'x','total'=>8]);
$add('create_allocated_sequence','create',[$f('number',['type'=>'core.sequence'])],[],[],['number'=>'INV-001']);
$add('create_caller_sequence','create',[$f('number',['type'=>'core.sequence'])],['number'=>'INV-001']);
$add('create_missing_sequence_required','create',[$f('number',['type'=>'core.sequence','required'=>true,'nullable'=>false])],[]);
$add('create_invalid_type_no_duplicate_required','create',[$f('amount',['type'=>'core.integer','required'=>true,'nullable'=>false])],['amount'=>'invalid']);
$add('create_invalid_default','create',[$f('amount',['type'=>'core.integer','default'=>'bad','required'=>true,'nullable'=>false])],[]);
$add('create_conditions_after_computation','create',[$f('name',['visibility_condition'=>$ref('allowed','boolean')]),$computed('allowed',$literal(true,'boolean'))],['name'=>'shown']);
$add('create_visibility_before_editability','create',[$f('name',['visibility_condition'=>$literal(false,'boolean'),'editability_condition'=>$literal(false,'boolean')])],['name'=>'hidden']);
$add('create_editability_false','create',[$f('name',['editability_condition'=>$literal(false,'boolean')])],['name'=>'locked']);
$add('create_condition_missing_dependency','create',[$f('name',['visibility_condition'=>$ref('missing','boolean')]),$f('missing',['type'=>'core.boolean'])],['name'=>'x','missing'=>'invalid']);
$add('create_formula_then_condition_then_required','create',[
    $computed('total',$ref('missing','integer')),
    $f('name',['visibility_condition'=>$literal(false,'boolean')]),
    $f('required',['required'=>true,'nullable'=>false]),$f('missing',['type'=>'core.integer'])],['name'=>'x','missing'=>'invalid']);
$add('update_input_order','update',[$f('name',['read_only'=>true]),$f('fixed',['immutable_after_create'=>true])],
    ['z_unknown'=>1,'fixed'=>'changed','name'=>'changed','a_unknown'=>2],['name'=>'before','fixed'=>'before']);
$add('update_immutable_same','update',[$f('fixed',['type'=>'core.integer','immutable_after_create'=>true])],['fixed'=>2],['fixed'=>2]);
$add('update_immutable_raw_type_matters','update',[$f('fixed',['type'=>'core.integer','immutable_after_create'=>true])],['fixed'=>'2'],['fixed'=>2]);
$add('update_condition_uses_prior_not_patch','update',[$f('enabled',['type'=>'core.boolean']),$f('name',['editability_condition'=>$ref('enabled','boolean')])],
    ['enabled'=>false,'name'=>'after'],['enabled'=>true,'name'=>'before']);
$add('update_condition_false_prior','update',[$f('enabled',['type'=>'core.boolean']),$f('name',['editability_condition'=>$ref('enabled','boolean')])],
    ['enabled'=>true,'name'=>'after'],['enabled'=>false,'name'=>'before']);
$add('update_normalization_failure_keeps_old','update',[$f('amount',['type'=>'core.integer','required'=>true,'nullable'=>false])],['amount'=>'invalid'],['amount'=>2]);
$add('update_codec_then_computation','update',[$f('amount',['type'=>'core.integer']),$computed('total',['op'=>'add','type'=>'integer','args'=>[$ref('amount','integer'),$literal(1,'integer')]])],['amount'=>'4'],['amount'=>2,'total'=>3]);
$isNull=static fn(string $name)=>['op'=>'is_null','type'=>'boolean','args'=>[$ref($name,'any')]];
$add('create_money_projects_null_for_condition','create',[
    $f('amount',['type'=>'core.money','precision'=>5,'scale'=>2]),$f('note',['visibility_condition'=>$isNull('amount')])],
    ['amount'=>['amount'=>'12.3','currency'=>'usd'],'note'=>'visible']);
$add('create_quantity_keeps_unit_and_scale','create',[$f('amount',['type'=>'core.quantity','precision'=>5,'scale'=>2])],
    ['amount'=>['amount'=>'12.3','unit'=>'kg']]);
$add('create_date_condition_uses_iso_projection','create',[
    $f('when',['type'=>'core.date']),$f('note',['visibility_condition'=>['op'=>'eq','type'=>'boolean',
        'args'=>[$ref('when','string'),$literal('2026-09-07T00:00:00.000000+00:00','string')]]])],
    ['when'=>'2026-09-07','note'=>'visible']);
$add('create_instant_retains_microseconds','create',[$f('when',['type'=>'core.instant'])],
    ['when'=>'2026-09-07T10:11:12.123456+02:00']);
$add('create_local_time_retains_date_anchor','create',[$f('when',['type'=>'core.local_time'])],
    ['when'=>'10:11:12.123456']);
$add('create_zoned_condition_uses_utc_projection','create',[
    $f('when',['type'=>'core.zoned_datetime']),$f('note',['visibility_condition'=>['op'=>'eq','type'=>'boolean',
        'args'=>[$ref('when','string'),$literal('2026-09-07T08:11:12.123456Z','string')]]])],
    ['when'=>['instant'=>'2026-09-07T08:11:12.123456Z','timezone'=>'Africa/Johannesburg'],'note'=>'visible']);
$sealed=new \Kumwe\App\BusinessRecord\Domain\EncryptedEnvelope('fixture-sealed',str_repeat('n',24),'fixture-key');
$add('create_presealed_envelope_stays_opaque','create',[
    $f('secret',['type'=>'core.secret','sensitivity'=>'secret']),$f('note',['visibility_condition'=>$isNull('secret')])],
    ['secret'=>$sealed,'note'=>'visible']);
$add('create_bounded_json_projects_null','create',[
    $f('payload',['type'=>'core.bounded_json']),$f('note',['visibility_condition'=>$isNull('payload')])],
    ['payload'=>['z'=>[true,2],'a'=>'first'],'note'=>'visible']);
$add('create_domain_object_fails_string_validator','create',[
    $f('amount',['type'=>'core.money','precision'=>5,'scale'=>2,'validators'=>[['rule'=>'min_length','value'=>0]]])],
    ['amount'=>['amount'=>'12.3','currency'=>'USD']]);
$add('update_immutable_array_same_order','update',[$f('payload',['type'=>'core.bounded_json','immutable_after_create'=>true])],
    ['payload'=>['a'=>1,'b'=>2]],['payload'=>['a'=>1,'b'=>2]]);
$add('update_immutable_array_reversed_order','update',[$f('payload',['type'=>'core.bounded_json','immutable_after_create'=>true])],
    ['payload'=>['b'=>2,'a'=>1]],['payload'=>['a'=>1,'b'=>2]]);
$add('update_immutable_array_integer_key_order','update',[$f('payload',['type'=>'core.bounded_json','immutable_after_create'=>true])],
    ['payload'=>[1=>'a',0=>'b']],['payload'=>[0=>'b',1=>'a']]);
$money=new \Kumwe\Conversion\Value\MoneyValue(\Kumwe\Conversion\Decimal\ExactDecimal::fromString('12.30',5,2),'USD');
$add('update_immutable_money_canonical_array_is_not_object','update',[
    $f('amount',['type'=>'core.money','precision'=>5,'scale'=>2,'immutable_after_create'=>true])],
    ['amount'=>['amount'=>'12.30','currency'=>'USD']],['amount'=>$money]);
$add('update_immutable_nested_domain_object_is_not_scalar','update',[
    $f('payload',['type'=>'core.bounded_json','immutable_after_create'=>true])],
    ['payload'=>['amount'=>'12.30']],['payload'=>['amount'=>\Kumwe\Conversion\Decimal\ExactDecimal::fromString('12.30',5,2)]]);
$moneyField=$f('amount',['type'=>'core.money','precision'=>5,'scale'=>2,'immutable_after_create'=>true]);
$add('update_immutable_money_same_instance','update',[$moneyField],['amount'=>$money],['amount'=>$money]);
$moneyCopy=new \Kumwe\Conversion\Value\MoneyValue(\Kumwe\Conversion\Decimal\ExactDecimal::fromString('12.30',5,2),'USD');
$add('update_immutable_money_distinct_instances','update',[$moneyField],['amount'=>$moneyCopy],['amount'=>$money]);
$arrayField=$f('payload',['type'=>'core.bounded_json','immutable_after_create'=>true]);
$add('update_immutable_nested_money_same_instance','update',[$arrayField],
    ['payload'=>['amount'=>$money]],['payload'=>['amount'=>$money]]);
$add('update_immutable_nested_money_distinct_instances','update',[$arrayField],
    ['payload'=>['amount'=>$moneyCopy]],['payload'=>['amount'=>$money]]);
$exact=\Kumwe\Conversion\Decimal\ExactDecimal::fromString('12.30',5,2);
$exactCopy=\Kumwe\Conversion\Decimal\ExactDecimal::fromString('12.30',5,2);
$exactField=$f('amount',['type'=>'core.decimal','precision'=>5,'scale'=>2,'immutable_after_create'=>true]);
$add('update_immutable_decimal_same_instance','update',[$exactField],['amount'=>$exact],['amount'=>$exact]);
$add('update_immutable_decimal_distinct_instances','update',[$exactField],['amount'=>$exactCopy],['amount'=>$exact]);
$when=new \DateTimeImmutable('2026-09-07T00:00:00+00:00');
$whenCopy=new \DateTimeImmutable('2026-09-07T00:00:00+00:00');
$whenField=$f('when',['type'=>'core.date','immutable_after_create'=>true]);
$add('update_immutable_datetime_same_instance','update',[$whenField],['when'=>$when],['when'=>$when]);
$add('update_immutable_datetime_distinct_instances','update',[$whenField],['when'=>$whenCopy],['when'=>$when]);
$path=$output;
file_put_contents($path,json_encode(['schema'=>'kumwe-document-preparation-corpus/v1',
    'source_app'=>'24ecf956423c18933e824b43cea1bfb9127a79a9','source_sdk'=>'e8ec23f155c5836c6bd083f154a8efb6e50aec66',
    'source_files_sha256'=>$sourceFiles,'source_conversion'=>\Composer\InstalledVersions::getReference('kumwe/conversion'),
    'oracle_php'=>PHP_VERSION,
    'oracle'=>'Unchanged App RecordRuleValidator::create/update with actual RecordValueCodec. Fixture compiler captures codec outcomes; it never reimplements preparation.',
    'input_contract'=>'Ordered caller entries retain original submitted value and explicit host normalization outcome. Tagged PHP arrays retain ordered typed-key entries recursively; domain tags retain runtime kind, canonical payload and execution-local positive instance token assigned by actual SplObjectStorage identity. Identity and allocated sequence values are host supplied.',
    'failure_contract'=>'Original PHP throws ordered findings and exposes no partial values; failed fixtures compare findings only.',
    'fixtures'=>$fixtures],JSON_PRETTY_PRINT|JSON_UNESCAPED_SLASHES|JSON_THROW_ON_ERROR)."\n");
echo count($fixtures)." preparation vectors frozen.\n";
