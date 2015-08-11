#include "Uber.h"
#include <iostream>
#include <map>
#include <mutex>
#include <set>
#include <vector>
#include <thrift/concurrency/ThreadManager.h>
#include <thrift/concurrency/PosixThreadFactory.h>
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TSimpleServer.h>
#include <thrift/server/TThreadPoolServer.h>
#include <thrift/server/TThreadedServer.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/transport/TTransportUtils.h>

using namespace ::apache::thrift;
using namespace ::apache::thrift::concurrency;
using namespace ::apache::thrift::protocol;
using namespace ::apache::thrift::server;
using namespace ::apache::thrift::transport;

using namespace ::std;
using namespace ::Uber;

// Defines all the methods of the Uber service.
// All public methods of this class are thread safe.
class UberHandler : virtual public UberIf {
 public:
  UberHandler() {
    num_current_trips = 0;
  }

  // Marks the beginning of a trip.
  // Since this function is very commonly executed, we make this O(1) by
  // pushing the trip points onto different vectors.
  void BeginTrip(const TripPoint& trip_point) {
    cout << "BeginTrip(" << trip_point << ")" << endl;

    // Add to the trip points vector.
    tp_mutex.lock();
    trip_points.push_back(trip_point);
    tp_mutex.unlock();

    // Add to the begin trip points vector.
    btp_mutex.lock();
    begin_trip_points.push_back(trip_point);
    btp_mutex.unlock();

    // Update number of current trips.
    nct_mutex.lock();
    num_current_trips++;
    nct_mutex.unlock();

    // Add to the timestamped number of trips vector.
    TimestampedNumTrips tnt;
    tnt.timestamp = time(NULL);
    tnt.num_trips = num_current_trips;
    tnt_mutex.lock();
    ts_num_trips.push_back(tnt);
    tnt_mutex.unlock();
  }

  // Updates a trip_point for a given trip.
  // Since this function is probably the most commonly executed function, we
  // make this O(1) by pushing the trip point onto a vector.
  void UpdateTrip(const TripPoint& trip_point) {
    cout << "UpdateTrip(" << trip_point << ")" << endl;

    // Add to the trip points vector.
    tp_mutex.lock();
    trip_points.push_back(trip_point);
    tp_mutex.unlock();
  }

  // Marks the ending of a trip.
  // Since this function is very commonly executed, we make this O(1) by
  // pushing the end trip points onto different vectors.
  void EndTrip(const TripPointAmount& trip_point_amount) {
    cout << "EndTrip(" << trip_point_amount << ")" << endl;
    TripPoint tp;
    tp.trip_id = trip_point_amount.trip_id;
    tp.point = trip_point_amount.point;

    // Add to the trip points vector.
    tp_mutex.lock();
    trip_points.push_back(tp);
    tp_mutex.unlock();

    // Add to the end trip points vector.
    etp_mutex.lock();
    end_trip_points.push_back(tp);
    etp_mutex.unlock();

    // Add the fare to the trip_id to fare map.
    titfm_mutex.lock();
    trip_id_to_fare_map[trip_point_amount.trip_id]
        = trip_point_amount.dollar_amount;
    titfm_mutex.unlock();

    nct_mutex.lock();
    num_current_trips--;
    nct_mutex.unlock();

    // Add to the timestamped num_trips vector.
    TimestampedNumTrips tnt;
    tnt.timestamp = time(NULL);
    tnt.num_trips = num_current_trips;
    tnt_mutex.lock();
    ts_num_trips.push_back(tnt);
    tnt_mutex.unlock();
  }

  // Returns the number of trips that pass through the given geo rectangle.
  // This function is O(num_trip_points).
  int32_t NumTripsPassed(const GeoRect& rectangle) {
    set<int> trips_passed;

    tp_mutex.lock();
    unsigned int num_trip_points = trip_points.size();
    tp_mutex.unlock();

    // Iterate over all the trip_points and gather the trip_ids in a set if
    // the trip point is within the given georect.
    for (unsigned int i = 0; i < num_trip_points; i++) {
      tp_mutex.lock();
      TripPoint& tp = trip_points[i];
      tp_mutex.unlock();
      if (PointIsWithinRect(tp.point, rectangle)) {
        trips_passed.insert(tp.trip_id);
      }
    }
    int num_trips_passed = trips_passed.size();
    cout << "NumTripsPassed(" << rectangle << ") = "
         << num_trips_passed << endl;
    return num_trips_passed;
  }

  // Returns the number of trips and the sum of their fares for all trips whose
  // starting or ending point is within the given geo-rectangle.
  // This function is O(num_trips).
  void NumTripsStartedOrStoppedAndFare(NumFare& _return, const GeoRect& rectangle) {
    set<int> trips_started_or_stopped;

    btp_mutex.lock();
    unsigned int num_begin_trip_points = begin_trip_points.size();
    btp_mutex.unlock();

    // Find trip_ids that have beginning trip points within the <rectangle>.
    for (unsigned int i = 0; i < num_begin_trip_points; i++) {
      btp_mutex.lock();
      TripPoint& tp = begin_trip_points[i];
      btp_mutex.unlock();

      if (PointIsWithinRect(tp.point, rectangle)) {
        trips_started_or_stopped.insert(tp.trip_id);
      }
    }

    etp_mutex.lock();
    unsigned int num_end_trip_points = end_trip_points.size();
    etp_mutex.unlock();

    // Find trip_ids that have ending trip points within the <rectangle>.
    for (unsigned int i = 0; i < num_end_trip_points; i++) {
      etp_mutex.lock();
      TripPoint& tp = end_trip_points[i];
      etp_mutex.unlock();

      if (PointIsWithinRect(tp.point, rectangle)) {
        trips_started_or_stopped.insert(tp.trip_id);
      }
    }
    _return.num_trips = trips_started_or_stopped.size();

    // Add up the total fare for all the given trip_ids.
    double total_fare = 0;
    for (set<int>::iterator it = trips_started_or_stopped.begin();
         it != trips_started_or_stopped.end(); ++it) {
      titfm_mutex.lock();
      total_fare += trip_id_to_fare_map[*it];
      titfm_mutex.unlock();
    }
    _return.dollar_fare = total_fare; 
    cout << "NumTripsStartedOrStoppedAndFare(" << rectangle << ") = "
         << _return << endl;
  }

  // Returns the number of trips occurring at the given timestamp.
  // This function is O(num_trips).
  int32_t NumOccurringTrips(const int64_t timestamp) {
    unsigned int i;

    // Get the vector size.
    tnt_mutex.lock();
    unsigned int num_ts_num_trips = ts_num_trips.size();
    tnt_mutex.unlock();

    // Iterate through the vector to find the num_trips with the largest
    // timestamp smaller than or equal to the given <timestamp>,
    for (i = 0; i < num_ts_num_trips; i++) {
      tnt_mutex.lock();
      bool ts_greater = ts_num_trips[i].timestamp > timestamp;
      tnt_mutex.unlock();

      if (ts_greater) break;
    }
  
    int num_occurring_trips = 0;  
    if (i > 0) {
      tnt_mutex.lock();
      num_occurring_trips = ts_num_trips[i - 1].num_trips;
      tnt_mutex.unlock();
    }
    cout << "NumOccurringTrips(" << timestamp << ") = " << num_occurring_trips << endl;
    return num_occurring_trips;
  }

 private:
  // Returns true if the given point is within the given geo-rectangle.
  bool PointIsWithinRect(const Point &pt, const GeoRect &gr) {
    return (pt.latitude >= gr.top_left.latitude &&
            pt.longitude >= gr.top_left.longitude &&
            pt.latitude <= gr.bottom_right.latitude &&
            pt.longitude <= gr.bottom_right.longitude);
  }

  // Following ar e the class members and the mutexes that guard concurrent
  // accesses to them.

  // Number of current trips.
  int num_current_trips;
  mutex nct_mutex;

  // Vector containing all the trip points (including beginning and end).
  vector<TripPoint> trip_points;
  mutex tp_mutex;

  // Vector containing only the beginning trip points.
  vector<TripPoint> begin_trip_points;
  mutex btp_mutex;

  // Vector containing only the ending trip points.
  vector<TripPoint> end_trip_points;
  mutex etp_mutex;

  // Map from the trip_id to the dollar fare of the trip.
  map<int, double> trip_id_to_fare_map;
  mutex titfm_mutex;

  // Vector containing num_trips at a given timestamp.
  vector<TimestampedNumTrips> ts_num_trips;
  mutex tnt_mutex;
};

int main(int argc, char **argv) {
  // Parse the thread pool worker count from the commandline if specified.
  int workerCount = 300;
  if (argc == 2) {
    workerCount = atoi(argv[1]);
  } else {
    cout << "You can optionally specify ./Uber_server <thread_pool_worker_count>"
         << endl;
  }

  const int kPort = 9090;
  boost::shared_ptr<UberHandler> handler(new UberHandler());
  boost::shared_ptr<TProcessor> processor(new UberProcessor(handler));
  boost::shared_ptr<TServerTransport> serverTransport(new TServerSocket(kPort));
  boost::shared_ptr<TTransportFactory> transportFactory(new TBufferedTransportFactory());
  boost::shared_ptr<TProtocolFactory> protocolFactory(new TBinaryProtocolFactory());

  // Start a thread pool server with the given number of thread pool workers.
  boost::shared_ptr<ThreadManager> threadManager =
    ThreadManager::newSimpleThreadManager(workerCount);
  boost::shared_ptr<PosixThreadFactory> threadFactory =
    boost::shared_ptr<PosixThreadFactory>(new PosixThreadFactory());
  threadManager->threadFactory(threadFactory);
  threadManager->start();
  TThreadPoolServer server(processor,
                           serverTransport,
                           transportFactory,
                           protocolFactory,
                           threadManager);

  server.serve();
  return 0;
}

