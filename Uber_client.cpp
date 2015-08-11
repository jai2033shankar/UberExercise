#include "Uber.h"

#include <iostream>
#include <transport/TSocket.h>
#include <transport/TBufferTransports.h>
#include <protocol/TBinaryProtocol.h>
#include <unistd.h>

using namespace apache::thrift;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;

using namespace std;
using namespace Uber;

int main(int argc, char **argv) {
  if (argc != 3) {
    cout << "Usage: ./Uber_client <trip_id> <trip_duration>" << endl;
    return 1;
  }

  int trip_id = atoi(argv[1]);
  int trip_duration = atoi(argv[2]);

  srand(time(NULL));
  usleep((rand() % 1000) * 1000);

  boost::shared_ptr<TSocket> socket(new TSocket("localhost", 9090));
  boost::shared_ptr<TTransport> transport(new TBufferedTransport(socket));
  boost::shared_ptr<TProtocol> protocol(new TBinaryProtocol(transport));

  UberClient client(protocol);
  transport->open();

  // Start the trip at (45, 90).
  double latitude = 45, longitude = 90;
  TripPoint tp_begin;
  tp_begin.trip_id = trip_id;
  tp_begin.point.latitude = latitude;
  tp_begin.point.longitude = longitude;
  client.BeginTrip(tp_begin);

  for (int i = 0; i < trip_duration; i++) {
    // Sleep for 1 to 2 second.
    usleep(1000 * 100 * (1 + rand() % 20));

    // Increase or decrease the lat/long randomly.
    latitude += ((double) rand() / RAND_MAX - 0.5);
    longitude += ((double) rand() / RAND_MAX - 0.5);

    TripPoint tpi;
    tpi.trip_id = trip_id;
    tpi.point.latitude = latitude;
    tpi.point.longitude = longitude;
    client.UpdateTrip(tpi);
  }

  latitude += ((double) rand() / RAND_MAX - 0.5);
  longitude += ((double) rand() / RAND_MAX - 0.5);

  TripPointAmount tpa_end;
  tpa_end.trip_id = trip_id;
  tpa_end.point.latitude = latitude;
  tpa_end.point.longitude = longitude;
  // Consider the trip duration as the $fare.
  tpa_end.dollar_amount = trip_duration;
  client.EndTrip(tpa_end);

  // Construct a geo rectangle.
  GeoRect gr;
  gr.top_left.latitude = 45;
  gr.top_left.longitude = 90;
  gr.bottom_right.latitude = 55;
  gr.bottom_right.longitude = 100;

  int num_trips = client.NumTripsPassed(gr);
  cout << trip_id << ": NumTripsPassed(" << gr << ") = " << num_trips << endl;

  NumFare nf;
  client.NumTripsStartedOrStoppedAndFare(nf, gr);
  cout << trip_id << ": NumTripsStartedOrStoppedAndFare(" << gr << ") = "
       << nf.num_trips << " fare = "<< nf.dollar_fare << endl;

  int64_t cur_time = time(NULL);
  int num_occurring_trips = client.NumOccurringTrips(cur_time);
  cout << trip_id << ": NumOccurringTrips(" << cur_time << ") = "
       << num_occurring_trips << endl;

  transport->close();

  return 0;
}
