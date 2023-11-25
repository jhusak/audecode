gcc tinywav.c audecode.c -o audecode
t=$?
if [ "$?" -eq 0 ] ; then
	echo SUCCESS
else
	echo ERRORS
fi
exit $t
