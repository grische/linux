#!/bin/sh
set -- \
	'age=5 order=0x1' \
	'age=5 order=0x2' \
	'age=5 order=0x4' \
	'age=5 order=0x8' \
	'age=5 order=0x10' \
	'age=5 order=0x20' \
	'age=5 order=0x40' \
	'age=5 order=0x80' \
	'age=5 order=0x100' \
	'age=5 ' \

echo "" > /var/tmp/allocstress.txt

while [ $# -gt 0 ]
do
	echo alloc $1 > /proc/avm/simulate
	shift;
	sleep 30;
	echo alloc stop > /proc/avm/simulate
	cat /proc/avm/allocstress >> /var/tmp/allocstress.txt
done
echo "alloc stop" > /proc/avm/simulate;
echo "result in /var/tmp/allocstress.txt"
cat /var/tmp/allocstress.txt
echo "---> results complete - todo:"
echo "tftp -p -l /var/tmp/allocstress.txt -r allocstress.txt 192.168.178.xx 10xx"
