BEGIN {
	print "const file_entry_t dir[] PROGMEM = {"
}

/^unsigned char/ {
	gsub(/\[\]/,"")
	print "{ name_" $3,",",$3,", &",$3"_len },"
}

END {
	print "{0,0,0},"
	print "};"
}
