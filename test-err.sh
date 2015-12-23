rate=100

for rate in `seq 0.0000001 0.000001 0.00001`
do
    echo ${rate}
    vagga waf --run "scratch/network --error-rate=${rate}" &> LOG
    grep CSV LOG > csv.csv
    cat csv.csv | awk -F, '{print $2}' | sort -u > streams.list
    mkdir -p ${rate}
    mv *.pcap ./${rate}/
    id=0
    for s in `cat streams.list`; do
        echo $s
        id=$((id+1))
        grep "$s" csv.csv > csv.$id.csv
        gnuplot -e "set output '${rate}/${rate}-$id-window.png'; set datafile separator ','; set term png; plot 'csv.$id.csv' using 3:4 with line;"
        gnuplot -e "set output '${rate}/${rate}-$id-threshold.png'; set datafile separator ','; set term png; plot 'csv.$id.csv' using 3:5 with line;"
    done
done

# cat streams.list | xargs -I % grep % csv.csv > csv.1.csv
# gnuplot -e 'set datafile separator ","; set output "t.png"; plot "csv.csv" using 3:4 with line; pause -1;'
