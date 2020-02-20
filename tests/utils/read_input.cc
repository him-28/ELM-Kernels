//! A set of utilities for testing ELM kernels in C++
#ifndef ELM_KERNEL_TEST_NETCDF_HH_
#define ELM_KERNEL_TEST_NETCDF_HH_

#include <sstream>
#include "read_input.hh"

#ifdef HAVE_MPI
using Comm_type = MPI_Comm;
#else
using Comm_type = int;
#endif

namespace ELM {
namespace IO {


//
// Readers for forcing data.
// -----------------------------------------------------------------------------

//
// Returns shape in a forcing file, as { N_TIMES, N_LAT_GLOBAL, N_LON_GLOBAL }
//
std::array<GO, 3>
get_forcing_dimensions(const Comm_type& comm,
                       const std::string& dir, const std::string& basename, const std::string& varname,
                       const Utils::Date& time_start, int n_months)
{
  std::array<GO,3> total_dims = { 0, 0, 0 };

  // files organized by month
  Utils::Date current(time_start);
  for (int mm=0; mm!=n_months; ++mm) {
    auto date = time_start.date();
    int month = std::get<1>(date);
    int year = std::get<0>(date);

    std::stringstream fname_full;
    fname_full << dir << "/" << basename << year << "-"
               << std::setw(2) << std::setfill('0') << month << ".nc";

    auto dims = get_dimensions<3>(comm, fname_full.str(), varname);

    if (mm == 0) {
      total_dims[1] = dims[1];
      total_dims[2] = dims[2];
    } else {
      assert(total_dims[1] == dims[1]);
      assert(total_dims[2] == dims[2]);
    }
    total_dims[0] += dims[0];

    current.increment_month();
  }
  return total_dims;
}


//
// Read a forcing file
//
// Requires shape(arr) == { N_TIMES, N_LAT_LOCAL, N_LON_LOCAL }
//
void
read_forcing(const std::string& dir, const std::string& basename, const std::string& varname,
             const Utils::Date& time_start, int n_months,
             const Utils::DomainDecomposition<2>& dd, Array<double,3>& arr)
{
  Utils::Date current(time_start);
  int i_times = 0;

  // my slice in space
  std::array<GO, 3> start = { 0, dd.start[0], dd.start[1] };
  std::array<GO, 3> count = { -1, dd.n_local[0], dd.n_local[1] };
  
  for (int mm=0; mm!=n_months; ++mm) {
    auto date = time_start.date();
    int month = std::get<1>(date);
    int year = std::get<0>(date);

    std::stringstream fname_full;
    fname_full << dir << "/" << basename << year << "-"
               << std::setw(2) << std::setfill('0') << month << ".nc";

    // this file's slice in time is the full thing
    auto dims = get_dimensions<3>(dd.comm, fname_full.str(), varname);
    count[0] = dims[0];

    // read the slice, into a specific location
    read(dd.comm, fname_full.str(), varname, start, count, arr[i_times].data());

    // increment the position and month
    i_times += dims[0];
    current.increment_month();
  }

}


//
// Readers for phenology data.
// -----------------------------------------------------------------------------

//
// Returns shape in a phenology file, as { N_TIMES, N_PFTS, N_LAT_GLOBAL, N_LON_GLOBAL }
//
std::array<GO, 4>
get_phenology_dimensions(const Comm_type& comm,
                         const std::string& dir, const std::string& basename, const std::string& varname,
                         const Utils::Date& time_start, int n_months)
{
  Utils::Date current(time_start);
  current.increment_month(n_months);
  assert(time_start.year == current.year && "No current support for crossing years in phenology data?");      

  std::stringstream fname_full;
  fname_full << dir << "/" << basename;
  auto phen_dims_one = get_dimensions<4>(comm, fname_full.str(), varname);
  phen_dims_one[0] = n_months;
  return phen_dims_one;
}


//
// Read a phenology file.
//
// Requires shape(arr) == { N_TIMES, N_PFTS, N_LAT_LOCAL, N_LON_LOCAL }
//
void
read_phenology(const std::string& dir, const std::string& basename, const std::string& varname,
               const Utils::Date& time_start, int n_months,
               const Utils::DomainDecomposition<2>& dd, Array<double,4>& arr)
{
  // my slice in space
  std::array<GO, 4> start = { std::get<1>(time_start.date())-1, 0, dd.start[0], dd.start[1] };
  std::array<GO, 4> count = { n_months, arr.extent(1), dd.n_local[0], dd.n_local[1] };

  std::stringstream fname_full;
  fname_full << dir << "/" << basename;

  read(dd.comm, fname_full.str(), varname, start, count, arr.data());
  return;
}

} // namespace Utils
} // namespace ELM


#endif
