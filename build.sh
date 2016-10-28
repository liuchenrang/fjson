phpize --clean && phpize && ./configure && make -j 4 && make install && ls `php -i|egrep ^extension_dir|head|awk '{print $3}'`
phpize --clean 
mv `php -i|egrep ^extension_dir|head|awk '{print $3}'`/json.so `php -i|egrep ^extension_dir|head|awk '{print $3}'`/fjson.so
php -r 'echo fjson_encode(["xx"=>1]);';
php -r 'echo fjson_encode(["xx"=>1.1]);';
php -r 'echo fjson_encode(["xx"=>null]);';
php -r 'echo fjson_encode(["xx"=>true]);';
php -r 'echo fjson_encode(["xx"=>false]);';