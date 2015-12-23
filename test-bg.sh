rate=100

for rate in `seq 1 20 120`
do
    echo ${rate}kbps
    vagga waf --run "scratch/network --bgRate=${rate}kbps" &> LOG
    grep CSV LOG > csv.csv
    cat csv.csv | awk -F, '{print $2}' | sort -u > streams.list
    mkdir -p ${rate}kbps
    mv *.pcap ./${rate}kbps/
    id=0
    for s in `cat streams.list`; do
        echo $s
        id=$((id+1))
        grep "$s" csv.csv > csv.$id.csv
        gnuplot -e "set output '${rate}kbps/${rate}kbps-$id-window.png'; set datafile separator ','; set term png; plot 'csv.$id.csv' using 3:4 with line;"
        gnuplot -e "set output '${rate}kbps/${rate}kbps-$id-threshold.png'; set datafile separator ','; set term png; plot 'csv.$id.csv' using 3:5 with line;"
    done
done

# cat streams.list | xargs -I % grep % csv.csv > csv.1.csv
# gnuplot -e 'set datafile separator ","; set output "t.png"; plot "csv.csv" using 3:4 with line; pause -1;'
