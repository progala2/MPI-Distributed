#include <mpi.h>
#include <iostream>
#include <list>
#include <cmath>
#include <cstdlib>
#include <unistd.h>

struct Coord {
	int x, y;
};

const char* messages[] = { "Waiting", "Playing", "Done" };

int main(int argc, char *argv[]) {
	int numtasks = atoi(argv[1]);

	const int REORDER = 1;
	const int END = 2;
	const int WANT = 3;
	const int WAITING = 0;
	const int PLAY = 1;

	MPI_Init(&argc, &argv);
	int rank;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	Coord *Cords = new Coord[numtasks];
	FILE *fr;
	fr = fopen(argv[2], "rt");
	for (int i = 0; i < numtasks; i++) {
		char line[80];
		fgets(line, 80, fr);
		sscanf(line, "%d %d", &Cords[i].x, &Cords[i].y);
	};
	fclose(fr);

	std::list<int> nbrs[numtasks];
	int edgeCount = 0;
	int indexes[numtasks];
	for (int i = 0; i < numtasks; ++i) {
		for (int j = 0; j < numtasks; ++j) {
			if (i == j)
				continue;
			float x2 = Cords[i].x - Cords[j].x;
			float y2 = Cords[i].y - Cords[j].y;
			float res = std::sqrt(x2 * x2 + y2 * y2);
			if (res <= 3) {
				edgeCount++;
				nbrs[i].push_back(j);
			}
		}
		indexes[i] = edgeCount;
	}
	delete[] Cords;
	int *edges = new int[edgeCount];

	for (int i = 0, k = 0; i < numtasks; ++i) {
		for (std::list<int>::iterator j = nbrs[i].begin(); j != nbrs[i].end(); ++j) {
			edges[k] = *j;
			++k;
		}
		nbrs[i].clear();
	}

	/*if (rank == 0) {
	 for (int i = 0; i < numtasks; ++i) {
	 std::cout << indexes[i] << std::endl;
	 }
	 for (int i = 0; i < edgeCount; ++i) {
	 std::cout << edges[i] << std::endl;
	 }
	 }*/

	MPI_Comm graphcomm;
	MPI_Graph_create(MPI_COMM_WORLD, numtasks, indexes, edges, REORDER, &graphcomm);
	delete[] edges;
	MPI_Comm_rank(graphcomm, &rank);

	int neighborsCount;
	MPI_Graph_neighbors_count(graphcomm, rank, &neighborsCount);
	int *neighbors = new int[neighborsCount];
	MPI_Graph_neighbors(graphcomm, rank, neighborsCount, neighbors);

	for (int i = 0; i < neighborsCount; ++i) {
		std::cout << "rank: " << rank << " Neighbour-" << i << ": " << neighbors[i] << std::endl;
		std::flush(std::cout);
	}

	// it's only for debugging purposes.
	MPI_Barrier(graphcomm);
	if (rank == 0) {
		std::cout << std::endl;
		std::flush(std::cout);
	}
	MPI_Barrier(graphcomm);

	MPI_Request* requests = new MPI_Request[neighborsCount];
	int k = 0;
	int message = PLAY;
	int* msgsRcv = new int[neighborsCount];
	memset(msgsRcv, WANT, sizeof(int) * neighborsCount);
	while (true) {
		for (int i = 0; i < neighborsCount; ++i) {
			if (msgsRcv[i] == END)
				continue;
			if (neighbors[i] < rank) {
				MPI_Recv(&msgsRcv[i], 1, MPI_INT, neighbors[i], 1, graphcomm, MPI_STATUSES_IGNORE );
				if (msgsRcv[i] == PLAY) {
					message = WAITING;
				}
			} else {
				MPI_Send(&message, 1, MPI_INT, neighbors[i], 1, graphcomm);
				std::cout << k << " rank: " << rank << " send to neighbour: " << neighbors[i]
						<< " message: " << messages[message] << std::endl;
				std::flush(std::cout);
			}
		}
		if (message == PLAY) {
			std::cout << k << " rank= " << rank << " is playing" << std::endl;
			message = END;
			std::flush(std::cout);
			sleep(2);
		}
		for (int i = 0; i < neighborsCount; ++i) {
			if (msgsRcv[i] == END) {
				requests[i] = MPI_REQUEST_NULL;
				continue;
			}
			if (message == END) {
				MPI_Isend(&message, 1, MPI_INT, neighbors[i], 1, graphcomm, &requests[i]);
				std::cout << k << " rank: " << rank << " send to neighbour: " << neighbors[i]
						<< " message: " << messages[message] << std::endl;
				std::flush(std::cout);
			} else if (msgsRcv[i] == PLAY) {
				MPI_Irecv(&msgsRcv[i], 1, MPI_INT, neighbors[i], 1, graphcomm, &requests[i]);
			} else {
				requests[i] = MPI_REQUEST_NULL;
			}
		}
		MPI_Waitall(neighborsCount, requests, MPI_STATUSES_IGNORE );
		if (message == END) {
			std::cout << k << " rank= " << rank << " have done" << std::endl;
			std::flush(std::cout);
			break;
		}

		message = PLAY;
		++k;
	}

	delete[] msgsRcv;
	delete[] requests;
	delete[] neighbors;
	MPI_Finalize();
	return 0;
}
