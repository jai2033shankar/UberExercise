#!/usr/local/bin/thrift --gen cpp
// This is the thrift file which defines the data structures and the
// communication protocol between UberServer and UberClient.

namespace cpp Uber

// Represents a geo-located point.
struct Point {
  1: double latitude,
  2: double longitude,
}

// Represents a geo-located point corresponding to a trip_id.
struct TripPoint {
  1: i32 trip_id,
  2: Point point,
}

// Represents the ending trip point of trip_id and the dollar amount fare.
struct TripPointAmount {
  1: i32 trip_id,
  2: Point point,
  3: double dollar_amount,
}

// Represents a geo-rectangle.
// For simplicity we only use the top left and bottom right points to represent
// it; instead of four arbitrary points.
struct GeoRect {
  1: Point top_left,
  2: Point bottom_right,
}

// Represents the total number of trips and their dollar fare.
struct NumFare {
  1: i32 num_trips,
  2: double dollar_fare,
}

// Represents the number of trips occurring at given timestamp.
struct TimestampedNumTrips {
  1: i64 timestamp,
  2: i32 num_trips,
}

// Represents the main Uber service.
service Uber {
  // Marks the beginning of a trip.
  void BeginTrip(1:TripPoint trip_point)

  // Updates a trip_point for a given trip.
  void UpdateTrip(1:TripPoint trip_point)

  // Marks the end of a trip.
  void EndTrip(1:TripPointAmount trip_point_amount)

  // Returns the number of trips that pass through the given geo-rectangle.
  i32 NumTripsPassed(1:GeoRect rectangle)

  // Returns the number of trips and the sum of their fares for all trips whose
  // starting or ending point is within the given geo-rectangle.
  NumFare NumTripsStartedOrStoppedAndFare(1:GeoRect rectangle)

  // Returns the number of trips occurring at a given timestamp.
  i32 NumOccurringTrips(1:i64 timestamp)  
}
