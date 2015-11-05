#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/time.h>
#include <mpi.h>

MPI_Status status;

void masterProcess(int n_proc) {
    printf("I am the master with %d slaves\n", n_proc);
}

void slaveProcess(int p_id) {
    printf("I am slave #%d\n", p_id);
}

int main(int argc, char* argv[]) {
    MPI_Status status;
    int n_proc;
    int p_id;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &n_proc);
    MPI_Comm_rank(MPI_COMM_WORLD, &p_id);    

    switch (p_id) {
    case 0:
        masterProcess(n_proc);
        break;
    default:
        slaveProcess(p_id);
        break;
    }
    MPI_Finalize();
}