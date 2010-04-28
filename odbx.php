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
var_dump($res = odbx_query("SELECT count(*) FROM odbxtest"));
var_dump($res2 = odbx_query("SELECT * FROM odbxtest"));
echo "\n-----------\n";
var_dump(odbx_column_count($res2));
var_dump(odbx_column_name($res2, 0));
var_dump(odbx_column_type($res2, 0));
var_dump(odbx_field_length($res2, 0));
var_dump(odbx_field_value($res2, 0));

var_dump(odbx_column_name($res2, 1));
var_dump(odbx_column_type($res2, 1));
var_dump(odbx_field_length($res2, 1));
var_dump(odbx_field_value($res2, 1));
echo "\n-----------\n";
/*
while(1 && true)
{
	var_dump($a = odbx_init("pgsql", "127.0.0.1","1234",true));
	sleep(1);
}
*/
