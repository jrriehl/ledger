#pragma once

#include"core/random/lcg.hpp"
#include"python/fetch_pybind.hpp"

namespace fetch
{
namespace random
{

void BuildLinearCongruentialGenerator(pybind11::module &module) {
  namespace py = pybind11;
  py::class_<LinearCongruentialGenerator>(module, "LinearCongruentialGenerator" )
    .def(py::init<>()) /* No constructors found */
    .def("Reset", &LinearCongruentialGenerator::Reset)
    .def("operator()", &LinearCongruentialGenerator::operator())
    .def("Seed", ( uint64_t (LinearCongruentialGenerator::*)() const ) &LinearCongruentialGenerator::Seed)
    .def("Seed", ( uint64_t (LinearCongruentialGenerator::*)(const fetch::random::LinearCongruentialGenerator::random_type &) ) &LinearCongruentialGenerator::Seed)
    .def("AsDouble", &LinearCongruentialGenerator::AsDouble);

}
};
};
