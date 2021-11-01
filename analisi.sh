#!/bin/bash
echo "┌──────────────────────┬──────────────────────┬──────────────────────┬──────────────────────┬────────────────┬────────────────┐"
printf "│ %-20s │ %-20s │ %-20s │ %-20s │ %s │ %-14s │\n" "CUSTOMER ID" "N PRODUCTS" "TIME IN SUPERMARKET" "TIME IN QUEUE" "N QUEUE VIEWED" "GOT SERVED"
echo "├──────────────────────┼──────────────────────┼──────────────────────┼──────────────────────┼────────────────┼────────────────┤"

while IFS=' ' read -r t id n_prod q_viewed sm_time q_time got_served; do

    if [ "$t" != "Customer:" ]; then
        continue
    fi

    sm_time_sec=$(echo "scale=3; $sm_time / 1000" | bc)
    q_time_sec=$(echo "scale=3; $q_time / 1000" | bc)
    printf "│ %-20s │ %-20s │ %-20s │ %-20s │ %-14s │ %-14s │\n" "$id" "$n_prod" "$sm_time_sec" "$q_time_sec" "$q_viewed" "$got_served"

done < $1
echo "└──────────────────────┴──────────────────────┴──────────────────────┴──────────────────────┴────────────────┴────────────────┘"

echo 
echo
echo "┌─────────────────┬─────────────────┬─────────────────┬─────────────────┬───────────────────────┬────────────┐"
printf "│ %-15s │ %-15s │ %-15s │ %-15s │ %-21s │ %s │\n" "CASH ID" "N PRODUCTS" "N CUSTOMERS" "TOT TIME OPEN" "AVERAGE SERVICE TIME" "N CLOSURES"
echo "├─────────────────┼─────────────────┼─────────────────┼─────────────────┼───────────────────────┼────────────┤"

while IFS=' ' read -r t id n_prod n_customers n_closures t_customers t_closures; do 

    if [ "$t" != "Cashier:" ]; then
        continue
    fi

    if [ "$n_customers" -eq "0" ]; then
        service_avg=""
    else
        IFS="," read -ra time_customers <<< "$t_customers";
        sum=0
        for ((i = 0; i < $n_customers; i++)); do
            sum=$(expr ${time_customers[$i]} + $sum)
        done
        service_avg=$(expr $sum / $n_customers)
        service_avg=$(echo "scale=3; $service_avg / 1000" | bc)
    fi


    if [ "$n_closures" -eq "0" ]; then
        open_time=""
    else
        IFS=',' read -ra time_closures <<< "$t_closures";
        open_time=0
        for ((i = 0; i<$n_closures; i++)); do
            open_time=$(expr ${time_closures[$i]} + $open_time)
        done
        open_time=$(echo "scale=3; $open_time / 1000" | bc)
    fi
    
    printf "│ %-15s │ %-15s │ %-15s │ %-15s │ %-21s │ %-10s │\n" "$id" "$n_prod" "$n_customers" "$open_time" "$service_avg" "$n_closures"
    
done < $1
echo "└─────────────────┴─────────────────┴─────────────────┴─────────────────┴───────────────────────┴────────────┘"
