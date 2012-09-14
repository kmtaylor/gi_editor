first_line=`grep -n '| Start |' $1 \
	   | awk 'BEGIN {FS=":"} {print $1}'`
last_line=`grep -n 'Total Size' $1 | tail -1 \
	   | awk 'BEGIN {FS=":"} {print $1}'`

del_range="1,$((--first_line)) d; $((++last_line)),$ d"
echo Deleting lines: $del_range
sed "$del_range" $1 | sed '/Total Size/d' > $2
