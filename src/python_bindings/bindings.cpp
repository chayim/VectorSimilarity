#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>
#include "VecSim/vecsim.h"

namespace py = pybind11;

class PyVecSimIndex {
    public:
    PyVecSimIndex (const VecSimParams &params) {
        index =  VecSimIndex_New(&params);
    }

    void addVector(py::object input, size_t id) {
        py::array_t < float, py::array::c_style | py::array::forcecast > items(input);
        float *vector_data=(float *) items.data(0);
        VecSimIndex_AddVector(index, (void *) vector_data,  id);
    }

    py::object knn(py::object input, size_t k) {
        py::array_t < float, py::array::c_style | py::array::forcecast > items(input);
        float *vector_data=(float *) items.data(0);
        VecSimQueryResult* res =  VecSimIndex_TopKQuery(index, (void*) vector_data, k);
        if (VecSimQueryResult_Len(res) != k) {
            throw std::runtime_error("Cannot return the results in a contigious 2D array. Probably ef or M is too small");
        }
        size_t *data_numpy_l = new size_t[k];
        float* data_numpy_d = new float[k];
        for (int i = 0; i < k; i++) {
            data_numpy_d[i] = res[i].score;
            data_numpy_l[i] = res[i].id;
        }
        py::capsule free_when_done_l(data_numpy_l, [](void *f) {
            delete[] f;
        });
        py::capsule free_when_done_d(data_numpy_d, [](void *f) {
            delete[] f;
        });
        return py::make_tuple(
            py::array_t<size_t>(
                    {(size_t)1, k}, // shape
                    {k * sizeof(size_t), sizeof(size_t)}, // C-style contiguous strides for double
                    data_numpy_l, // the data pointer
                    free_when_done_l),
            py::array_t<float>(
                    {(size_t)1, k}, // shape
                    {k * sizeof(float), sizeof(float)}, // C-style contiguous strides for double
                    data_numpy_d, // the data pointer
                    free_when_done_d));
    }

    size_t indexSize(){
        return VecSimIndex_IndexSize(index);
    }

    virtual ~PyVecSimIndex() {
        VecSimIndex_Free(index);
    }
    private:
        VecSimIndex* index;
};


PYBIND11_MODULE(VecSim, m) {
    py::enum_<VecSimAlgo>(m, "VecSimAlgo")
    .value("VecSimAlgo_HNSW", VecSimAlgo_HNSW)
    .value("VecSimAlgo_BF", VecSimAlgo_BF)
    .export_values();

    py::enum_<VecSimType>(m, "VecSimType")
    .value("VecSimType_FLOAT32", VecSimType_FLOAT32)
    .value("VecSimType_FLOAT64", VecSimType_FLOAT64)
    .value("VecSimType_INT32", VecSimType_INT32)
    .value("VecSimType_INT64", VecSimType_INT64)
    .export_values();

    py::enum_<VecSimMetric>(m, "VecSimMetric")
    .value("VecSimMetric_L2", VecSimMetric_L2)
    .value("VecSimMetric_IP", VecSimMetric_IP)
    .export_values();

    py::class_<HNSWParams>(m, "HNSWParams")
    .def(py::init())
    .def_readwrite("initialCapacity", &HNSWParams::initialCapacity)
    .def_readwrite("M", &HNSWParams::M)
    .def_readwrite("efConstruction", &HNSWParams::efConstruction);

    py::class_<VecSimParams>(m, "VecSimParams")
    .def(py::init())
    .def_readwrite("algo", &VecSimParams::algo)
    .def_readwrite("type", &VecSimParams::type)
    .def_readwrite("dim", &VecSimParams::size)
    .def_readwrite("metric", &VecSimParams::metric)
    .def_readwrite("hnswParams", &VecSimParams::hnswParams)
    .def_readwrite("bfParams", &VecSimParams::bfParams);

    py::class_<PyVecSimIndex>(m, "VecSimIndex")
    .def(py::init([](const VecSimParams& params) {return new PyVecSimIndex(params);}), py::arg("params"))
    .def("add_vector", &PyVecSimIndex::addVector)
    .def("knn_query", &PyVecSimIndex::knn, py::arg("vector"), py::arg("id"))
    .def("index_size", &PyVecSimIndex::indexSize);
}
