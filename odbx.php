<?php
error_reporting(E_ALL);
//var_dump($a = odbx_init("pgsql", "127.0.0.2","1234",true));
var_dump($a = odbx_init("pgsql"));
var_dump($a2 = odbx_init("mysql"));
//var_dump(odbx_finish());
echo ODBX_BIND_SIMPLE;
echo "\n";
echo "\n-----------\n";
var_dump($b = odbx_bind($a, "odbx", "postgres", "", ODBX_BIND_SIMPLE));
var_dump($b2 = odbx_bind($a2, "odbx", "root", "", ODBX_BIND_SIMPLE));
echo "\n-----------\n";
//var_dump($b = odbx_unbind());
echo "\n-----------\n";
var_dump(odbx_error($b));
echo "\n-----------\n";
var_dump(odbx_escape($a, "12345'67890"));
var_dump(odbx_escape($a2, "12345'67890"));
echo "\n-----------\n";
var_dump(odbx_set_option($a, ODBX_OPT_TLS, "12345'67890"));
var_dump(odbx_get_option($a, ODBX_OPT_TLS, "12345'67890"));
var_dump(odbx_get_option($a2, ODBX_OPT_TLS, "12345'67890"));
var_dump(odbx_error(-13));
echo "\n*********************\n";
var_dump($res = odbx_query("insert into odbxtest(b) values(". time() .")"));
//var_dump($res = odbx_query("update odbxtest set b=". time()));
echo "\n-----------\n";
var_dump(odbx_rows_affected($res));
var_dump($result = odbx_query("SELECT count(*) FROM odbxtest"));
var_dump($result2 = odbx_query("SELECT * FROM odbxtest limit 100"));
echo "\n-----------\n";

//var_dump(odbx_row_fetch($result));
echo "\n========= result ======================\n";
while (odbx_row_fetch($result)) {
	for( $i = 0; $i < odbx_column_count( $result ); $i++ ) {
		printf( "Name: %s\n", odbx_column_name( $result, $i ) );
		printf( "Type: %d\n", odbx_column_type( $result, $i ) );
		printf( "Length: %d\n", odbx_field_length( $result, $i ) );
		printf( "Value: %s\n", odbx_field_value( $result, $i ) );
		echo "\n-----------\n";
	}
}

echo "\n========= result2 ======================\n";
while (odbx_row_fetch($result2)) {
	for( $i = 0; $i < odbx_column_count( $result2 ); $i++ ) {
		printf( "Name: %s\n", odbx_column_name( $result2, $i ) );
		printf( "Type: %d\n", odbx_column_type( $result2, $i ) );
		printf( "Length: %d\n", odbx_field_length( $result2, $i ) );
		printf( "Value: %s\n", odbx_field_value( $result2, $i ) );
		echo "\n-----------\n";
	}
}
echo "\n===============================\n";
