# fjson
force json format data 
php -r 'echo fjson_encode(["xx"=>1]);';
php -r 'echo fjson_encode(["xx"=>1.1]);';
php -r 'echo fjson_encode(["xx"=>null]);';
php -r 'echo fjson_encode(["xx"=>true]);';
php -r 'echo fjson_encode(["xx"=>false]);';


php -r 'echo json_encode(["xx"=>1]);';
php -r 'echo json_encode(["xx"=>1.1]);';
php -r 'echo json_encode(["xx"=>null]);';
php -r 'echo json_encode(["xx"=>true]);';
php -r 'echo json_encode(["xx"=>false]);';