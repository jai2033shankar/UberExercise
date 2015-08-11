# This shell script can be used to start multiple Uber clients and load test
# the Uber server's scalability.

if [ "$#" -ne 1 ]; then
  echo "Usage: $0 <num_clients>"
  exit 1
fi

num_clients=$1

for i in `seq 1 $num_clients`; do
  duration=$(( ( RANDOM % 20 )  + 1 ))
  ./Uber_client $i $duration &
done
