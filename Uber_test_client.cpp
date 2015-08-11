#include "Uber.h"

#include <iostream>
#include <transport/TSocket.h>
#include <transport/TBufferTransports.h>
#include <protocol/TBinaryProtocol.h>

using namespace apache::thrift;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;

using namespace std;
using namespace Uber;

// This file constructs an Uber test client and tests the basic functionalty
// of the Uber service.
int main(int argc, char **argv) {
  boost::shared_ptr<TSocket> socket(new TSocket("localhost", 9090));
  boost::shared_ptr<TTransport> transport(new TBufferedTransport(socket));
  boost::shared_ptr<TProtocol> protocol(new TBinaryProtocol(transport));

  UberClient client(protocol);
  transport->open();

  // Begin trip 7
  TripPoint tp1;
  tp1.trip_id = 7;
  tp1.point.latitude = 1.01;
  tp1.point.longitude = 2.02;
  client.BeginTrip(tp1);

  // Begin trip 11
  TripPoint tp2;
  tp2.trip_id = 11;
  tp2.point.latitude = 5;
  tp2.point.longitude = 6;
  client.BeginTrip(tp2);

  // Construct a geo rectangle
  GeoRect gr;
  gr.top_left.latitude = 1;
  gr.top_left.longitude = 2;
  gr.bottom_right.latitude = 3;
  gr.bottom_right.longitude = 4;

  // Trip 7 passes through the georect
  int num_trips = client.NumTripsPassed(gr);
  cout << "Num trips passing through georect = " << num_trips << endl;
  assert(num_trips == 1);

  // End trip 7 outside the georect with fare $13.50
  TripPointAmount tpa;
  tpa.trip_id = 7;
  tpa.point.latitude = 7;
  tpa.point.longitude = 8;
  tpa.dollar_amount = 13.5;
  client.EndTrip(tpa);

  // Only trip 7 started in the georect
  NumFare nf;
  client.NumTripsStartedOrStoppedAndFare(nf, gr);
  cout << "Num trips = " << nf.num_trips << " fare = "<< nf.dollar_fare << endl;
  assert(nf.num_trips == 1);
  assert(nf.dollar_fare == 13.5);

  // Only trip 11 is currently occurring
  int64_t cur_time = time(NULL);
  int num_occurring_trips = client.NumOccurringTrips(cur_time);
  cout << "Num occurring trips = " << num_occurring_trips << endl;
  assert(num_occurring_trips == 1);

  // End trip 11
  tpa.trip_id = 11;
  tpa.point.latitude = 7;
  tpa.point.longitude = 8;
  tpa.dollar_amount = 3.5;
  client.EndTrip(tpa);

  // No trips are running now
  cur_time += 10;
  num_occurring_trips = client.NumOccurringTrips(cur_time);
  cout << "Num occurring trips = " << num_occurring_trips << endl;
  assert(num_occurring_trips == 0);

  transport->close();

  return 0;
}
