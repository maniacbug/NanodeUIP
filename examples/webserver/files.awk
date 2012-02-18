/^unsigned char/ {
	print "extern const",$2,$3,";"
	print "const char name_" $3,"PROGMEM = \"/" gensub(/.*\/([^\/]*).c.?.?$/,"\\1","g",FILENAME) "\";"
}
/^unsigned int/ {
	print "extern const",$1,$2,$3,";"
}
