/unsigned/ { print $1,$2,$3,"__attribute__((__section__(\".progmem.data\")))",$4,$5; next } 
{ print }
