# UberExercise
A simplistic version of the Uber backend client and server.

The communication protocol between the client and the server is defined in `uber.thrift` file. The main files are `Uber_client.cpp`, `Uber_server.cpp` and `Uber_test_client.cpp`.

To compile the client and server, you need to install thrift (https://thrift.apache.org/docs/install/) and then type
`make`.

To start the server, type `./Uber_server <thread_pool_worker_count>`. This starts a thrift server with a thread pool containing the given number of workers. See `Uber_server.cpp` for details.

To start the client, type `./Uber_client <trip_id> <trip_duration>`. This generates data for a given trip id and duration. See `Uber_client.cpp` for details.

To load test the server, type `sh load_test.sh <num_clients>` to start a large number of clients. The current implementation scales to more than 1000 parallel clients on a `t2.medium` Amazon EC2 VM instance which has 2 vCPUs.
