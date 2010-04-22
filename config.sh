phpize

./configure \
--with-php-config=/usr/local/bin/php-config \
--with-apxs=/usr/sbin/apxs \
--enable-odbx

make clean
make
make install
