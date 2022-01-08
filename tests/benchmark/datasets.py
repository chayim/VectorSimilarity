import os
from urllib.request import urlretrieve
import numpy as np
import time
from VecSim import *
import h5py
from scipy import spatial


def get_vecsim_metric(dataset_metric):
    metrics_map = {'angular': VecSimMetric_Cosine, 'euclidean': VecSimMetric_L2}
    return metrics_map[dataset_metric]


def download(src, dst):
    if not os.path.exists(dst):
        print('downloading %s -> %s...' % (src, dst))
        urlretrieve(src, dst)


# Download dataset from ann-benchmarks, save the file locally
def get_data_set(dataset_name):
    if not os.path.exists('data'):
        os.mkdir('data')
    hdf5_filename = os.path.join('data', '%s.hdf5' % dataset_name)
    url = 'http://ann-benchmarks.com/%s.hdf5' % dataset_name
    download(url, hdf5_filename)
    return h5py.File(hdf5_filename, 'r')


# Create an HNSW index from dataset based on specific params.
def create_hnsw_index(dataset, ef_construction, M):
    X_train = np.array(dataset['train'])
    distance = dataset.attrs['distance']
    dimension = int(dataset.attrs['dimension']) if 'dimension' in dataset.attrs else len(X_train[0])
    print('got a train set of size (%d * %d)' % (X_train.shape[0], dimension))
    print('metric is: %s' % distance)

    # Init index.
    hnswparams = HNSWParams()
    hnswparams.M = M
    hnswparams.efConstruction = ef_construction
    hnswparams.initialCapacity = len(X_train)
    hnswparams.dim = dimension
    hnswparams.type = VecSimType_FLOAT32
    hnswparams.metric = get_vecsim_metric(distance)
    return HNSWIndex(hnswparams)


def populate_save_index(hnsw_index, index_file_name, X_train):
    if os.path.exists(index_file_name):
        print("Index already exist. Remove index file first to override it")
        return index_file_name
    # Build the index by inserting vectors to it one by one.
    t0 = time.time()
    for i, vector in enumerate(X_train):
        hnsw_index.add_vector(vector, i)
    print('Built index time:', (time.time() - t0)/60, "minutes")
    hnsw_index.save_index(index_file_name)
    return index_file_name


def measure_recall_per_second(hnsw_index, dataset, num_queries, k, ef_runtime):
    X_train = np.array(dataset['train'])
    X_test = np.array(dataset['test'])

    # Create BF index to compare hnsw results with
    bfparams = BFParams()
    bfparams.initialCapacity = len(X_train)
    bfparams.dim = int(dataset.attrs['dimension']) if 'dimension' in dataset.attrs else len(X_train[0])
    bfparams.type = VecSimType_FLOAT32
    bfparams.metric = get_vecsim_metric(dataset.attrs['distance'])
    bf_index = BFIndex(bfparams)

    # Add all the vectors in the train set into the index.
    for i, vector in enumerate(X_train):
        bf_index.add_vector(vector, i)
    assert bf_index.index_size() == hnsw_index.index_size()

    # Measure recall and times
    hnsw_index.set_ef(ef_runtime)
    print("Running queries with ef_runtime =", ef_runtime)
    correct = 0
    bf_total_time = 0
    hnsw_total_time = 0
    for target_vector in X_test[:num_queries]:
        start = time.time()
        hnsw_labels, hnsw_distances = hnsw_index.knn_query(target_vector, k)
        hnsw_total_time += (time.time() - start)
        start = time.time()
        bf_labels, bf_distances = bf_index.knn_query(target_vector, k)
        bf_total_time += (time.time() - start)
        for label in hnsw_labels[0]:
            for correct_label in bf_labels[0]:
                if label == correct_label:
                    correct += 1
                    break
    # Measure recall
    recall = float(correct)/(k*num_queries)
    print("\nrecall is:", recall)
    print("BF query per seconds: ", num_queries/bf_total_time)
    print("HNSW query per seconds: ", num_queries/hnsw_total_time)


def run_benchmark(dataset_name, ef_construction, M, ef_values):
    dataset = get_data_set(dataset_name)
    hnsw_index = create_hnsw_index(dataset, ef_construction, M)
    index_file_name = os.path.join('data', '%s-M=%s-ef=%s.hnsw' % (dataset_name, M, ef_construction))
    # If we are using existing index, we just take the existing file.
    populate_save_index(hnsw_index, index_file_name, np.array(dataset['train']))
    hnsw_index.load_index(index_file_name)
    for ef_runtime in ef_values:
        measure_recall_per_second(hnsw_index, dataset, num_queries=1000, k=10, ef_runtime=ef_runtime)


if __name__ == "__main__":
    DATASETS = ['glove-100-angular', 'glove-200-angular', 'mnist-784-euclidean', 'sift-128-euclidean']
    dataset_params = {'glove-100-angular': (250, 36, [150, 300, 500]),
                      'glove-200-angular': (350, 48, [200, 350, 600]),
                      'mnist-784-euclidean': (200, 36, [100, 200, 350]),
                      'sift-128-euclidean': (250, 36, [150, 300, 500])}
    for d_name in DATASETS:
        run_benchmark(d_name, *dataset_params[d_name])
