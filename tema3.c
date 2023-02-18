#include<mpi.h>
#include<stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

#define ROOT 0
#define COORD_NUM 4
// numarul si id-urile muncitorilor din clusterul unui coordonator
int num_neigh_worker[COORD_NUM], *neigh_worker[COORD_NUM]; 
// vecinul stang si vecinul drept al unui coordonator
int neigh_coord_left[COORD_NUM], neigh_coord_right[COORD_NUM];
int parent[COORD_NUM]; // parintele in arborele de acoperire
/**
 * @brief citeste conexiunile la vecini de tip muncitor
 * 
 * @param rank 
 */
void read_cluster(int rank) {
    
	if(rank >= COORD_NUM){// daca nu e coordonator
		printf("read_cluster: %d is not coordinator\n", rank);
		return;
	}

	FILE *fp;
    char file_name[15];
    sprintf(file_name, "cluster%d.txt", rank);

    fp = fopen(file_name, "r");
	fscanf(fp, "%d", &num_neigh_worker[rank]);

	neigh_worker[rank] = malloc(sizeof(int) * num_neigh_worker[rank]);

	for (size_t i = 0; i < num_neigh_worker[rank]; i++)
		fscanf(fp, "%d", &neigh_worker[rank][i]);
}
/**
 * @brief initializeaza topologia
 * 
 * @param rank id-ul procesului curent
 * @param top_error eroarea din topologie:
 * 0 - inel complet
 * 1 - lipseste conexiunea (0,1)
 * 2 - lipsesc conexiunile (0,1) si (1,2)
 */
void init(int rank, int top_error){
	for(int i = 0; i < COORD_NUM; i++) {
		// la inceput, nu se cunoaste nimic
		num_neigh_worker[i] = 0;
		neigh_worker[i] = NULL;
		neigh_coord_left[i] = -1;
		neigh_coord_right[i] = -1;
		parent[i] = -1;
	}
	switch(rank){ // procesele coordonatoare sunt initializate cu vecinii
		case 0: neigh_coord_left[0] = 3; neigh_coord_right[0] = 1; break;
		case 1: neigh_coord_left[1] = 0; neigh_coord_right[1] = 2; break;
		case 2: neigh_coord_left[2] = 1; neigh_coord_right[2] = 3; break;
		case 3: neigh_coord_left[3] = 2; neigh_coord_right[3] = 0; break;
	}
	if(top_error == 1){
		switch(rank){ // dispare conexiunea (0,1)
			case 0: neigh_coord_right[0] = -1;
			case 1: neigh_coord_left[1] = -1;
		}
	}
	else if(top_error == 2){
		switch(rank){ // dispar conexiunile (0,1) si (1,2)
			case 0: neigh_coord_right[0] = -1;
			case 1: neigh_coord_left[1] = -1; neigh_coord_right[1] = -1;
			case 2: neigh_coord_left[2] = -1;
		}
	}

	if(rank < 4)
		read_cluster(rank);
}


/**
 * @brief trimite un vector de int la un alt proces
 * 
 * @param rank id-ul procesului curent
 * @param dest id-ul procesului destinatie
 * @param size dimensiunea vectorului
 * @param data pointer la vectorul trimis
 */
void send_data(int rank, int dest, int size, int *data) {
	printf("M(%d,%d)\n", rank, dest);
	// trimite dimensiunea vectorului
	MPI_Send(&size, 1, MPI_INT, dest, 0, MPI_COMM_WORLD);  
	if(size > 0){
		printf("M(%d,%d)\n", rank, dest);
		// trimite vectorul
		MPI_Send(data, size, MPI_INT, dest, 0, MPI_COMM_WORLD);
	}

}
/**
 * @brief trimite o valoare de tip int
 * 
 * @param rank id-ul procesului curent
 * @param dest id-ul procesului destinatie
 * @param value valoarea trimisa
 */
void send_int(int rank, int dest, int value){
	send_data(rank, dest, 1, &value);
}
/**
 * @brief primeste un vector de int de la un alt proces si il scrie intr-un buffer
 * 
 * @param rank id-ul procesului curent
 * @param src id-ul procesului sursa sau MPI_ANY_SOURCE, daca se asteapta mesaj de la oricare proces
 * @param size pointer la dimensiunea vectorului primit 
 * @param buffer pointer la buffer-ul in care e scris vectorul primit
 * @return int id-ul procesului sursa (folositor cand nu se stie sursa)
 */
int recv_data_in_buffer(int rank, int src, int *size, int *buffer) {
	MPI_Status status;
	// primeste dimensiunea vectorului
	MPI_Recv(size, 1, MPI_INT, src, 0, MPI_COMM_WORLD, &status);
	if(*size > 0)
		// primeste vectorul
		MPI_Recv(buffer, *size, MPI_INT, status.MPI_SOURCE, 0, MPI_COMM_WORLD, NULL);
	return status.MPI_SOURCE;
}
/**
 * @brief primeste un vector de int de la un alt proces si il scrie intr-un spatiu alocat dinamic
 * 
 * @param rank id-ul procesului curent
 * @param src id-ul procesului sursa sau MPI_ANY_SOURCE, daca se asteapta mesaj de la oricare proces
 * @param size pointer la dimensiunea vectorului primit 
 * @param data pointer la spatiul in care e scris vectorul primit
 * @return int id-ul procesului sursa (folositor cand nu se stie sursa)
 */
int recv_data_dynamic(int rank, int src, int *size, int **data){

	MPI_Status status;
	// primeste dimensiunea vectorului
	MPI_Recv(size, 1, MPI_INT, src, 0, MPI_COMM_WORLD, &status);
	if(*size > 0){
		*data = malloc((*size) * sizeof(int));
		// primeste vectorul
		MPI_Recv(*data, *size, MPI_INT, status.MPI_SOURCE, 0, MPI_COMM_WORLD, NULL);
	} 
	else{
		*data = NULL;
	}
	
	return status.MPI_SOURCE;

}
/**
 * @brief primeste o valoare de tip int
 * 
 * @param rank id-ul procesului curent
 * @param src id-ul procesului sursa sau MPI_ANY_SOURCE, daca se asteapta mesaj de la oricare proces
 * @param value pointer la valoarea primita
 * @return int id-ul procesului de la care a primit valoarea
 */
int recv_int(int rank, int src, int *value){
	int size;
	return recv_data_in_buffer(rank, src, &size, value);
}
/**
 * @brief trimite topologia la un alt proces
 * 
 * @param rank id-ul procesului curent
 * @param dest id-ul procesului destinatie
 */
void send_topology(int rank, int dest){
	for(int i = 0; i < COORD_NUM; i++){
		send_data(rank, dest, num_neigh_worker[i], neigh_worker[i]);
	}
	send_data(rank, dest, COORD_NUM, neigh_coord_left);
	send_data(rank, dest, COORD_NUM, neigh_coord_right);
	send_data(rank, dest, COORD_NUM, parent);
}
/**
 * @brief primeste topologia de la un alt proces
 * 
 * @param rank id-ul procesului curent
 * @param src id-ul procesului sursa sau MPI_ANY_SOURCE, daca se asteapta mesaj de la oricare proces
 * @return int int id-ul procesului de la care a primit topologia
 */
int recv_topology(int rank, int src){
	
	int size;
	
	for(int i = 0; i < COORD_NUM; i++){
		int *temp;
		src = recv_data_dynamic(rank, src, &size, &temp);
		if(neigh_worker[i] == NULL){
			neigh_worker[i] = temp;
			num_neigh_worker[i] = size;
		}
		else{
			free(temp);
		}
	}
	int temp[COORD_NUM];
	recv_data_in_buffer(rank, src, &size, temp);
	for(int i = 0; i < COORD_NUM; i++){
		if(temp[i] != -1)
			neigh_coord_left[i] = temp[i];
	}
	recv_data_in_buffer(rank, src, &size, temp);
	for(int i = 0; i < COORD_NUM; i++){
		if(temp[i] != -1)
			neigh_coord_right[i] = temp[i];
	}
	recv_data_in_buffer(rank, src, &size, temp);
	for(int i = 0; i < COORD_NUM; i++){
		if(temp[i] != -1)
			parent[i] = temp[i];
	}
	return src;
}
/**
 * @brief distribuie topologia intre procese
 * 
 * @param rank id-ul procesului curent
 */
void distribute_topology(int rank){
	
	
	if(rank < COORD_NUM){
		// pas 1 : ditribuire topologie intre coordonatori
		if(neigh_coord_left[rank] != -1 || neigh_coord_right[rank] != -1) // nod neizolat
		{
			if(rank == ROOT)
				parent[rank] = -1;
			else{ // initializare arbore de acoperire
				parent[rank] = recv_topology(rank, MPI_ANY_SOURCE);
			}
			// trimitere topologie la vecini, cu exceptia parintelui
			if(neigh_coord_right[rank] != -1 && neigh_coord_right[rank] != parent[rank])
				send_topology(rank, neigh_coord_right[rank]);
			if(neigh_coord_left[rank] != -1 && neigh_coord_left[rank] != parent[rank])
				send_topology(rank, neigh_coord_left[rank]);

			// primire topologie la vecini, cu exceptia parintelui
			if(neigh_coord_right[rank] != -1 && neigh_coord_right[rank] != parent[rank])
				recv_topology(rank, neigh_coord_right[rank]);
			if(neigh_coord_left[rank] != -1 && neigh_coord_left[rank] != parent[rank])
				recv_topology(rank, neigh_coord_left[rank]);

			if(rank != ROOT){
				send_topology(rank, parent[rank]);
				recv_topology(rank, parent[rank]);
			}


			//trimitere la copii
			if(neigh_coord_right[rank] != -1 && parent[neigh_coord_right[rank]] == rank)
				send_topology(rank, neigh_coord_right[rank]);
			if(neigh_coord_left[rank] != -1 && parent[neigh_coord_left[rank]] == rank)
				send_topology(rank, neigh_coord_left[rank]);
		}


		
		// pas 2 : distribuire topologie la muncitori
		for(int i = 0; i < num_neigh_worker[rank]; i++)
			send_topology(rank, neigh_worker[rank][i]);
	}
	else // muncitorii primesc topologia
		recv_topology(rank, MPI_ANY_SOURCE);

}

/**
 * @brief afiseaza topologia memorata de un proces
 * 
 * @param rank 
 */
void print_topology(int rank){
	printf("%d -> ", rank);
	for(int i = 0; i < COORD_NUM; i++){
		if(num_neigh_worker[i] == 0) continue;
		printf("%d:", i);
		for(int j = 0; j < num_neigh_worker[i]; j++){
			if(j > 0) printf(",");
			printf("%d", neigh_worker[i][j]);
		}
		printf(" ");
	}
	printf("\n");
}

/**
 * @brief operatia efectuata asupra valorilor
 * 
 * @param x 
 * @return int 
 */
int op(int x){
	return x * 5;
}

/**
 * @brief alege x si y asa incat: 
 * l = x + y
 * x/a = y/b (aproximativ)
 * 
 * @param l 
 * @param a 
 * @param b 
 * @param x 
 * @param y 
 */
void proportional_partition(int l, int a, int b, int *x, int *y){
	double a1, a2, aux;
	aux = (l * 1.0 / (a + b) * a);
	a1 = (int) aux;
	a2 = a1 + 1;
	if(aux - a1 > a2 - aux)
		*x = a2;
	else 
		*x = a1;
	*y = l - *x;
}

/**
 * @brief partitioneaza un vector in bucati egale si 
 * il trimite muncitorilor din clusterul asociat
 * 
 * @param rank id-ul coordonatorului
 * @param size dimensiunea vetorului
 * @param data vectorul
 * @return int nr de valori trimise
 */
int send_data_to_workers(int rank, int size, int *data){
	
	if(rank >= COORD_NUM){
		printf("send_data_to_workers: %d is not coordinator\n", rank);
		return -1;
	}
	int chunk_size, remaining = size;
	int sent = 0;
	for(int i = 0; i < num_neigh_worker[rank]; i++){ // trimitere la muncitori
		proportional_partition(remaining, 1, num_neigh_worker[rank] - i - 1, &chunk_size, &remaining);
		
		send_data(rank, neigh_worker[rank][i], chunk_size, data + sent);
		sent += chunk_size;
	}
	return sent;
}
/**
 * @brief primeste un vector de la muncitori
 * 
 * @param rank id-ul procesului curent
 * @param data vectorul cu datele primite
 * @return int nr de valori primite
 */
int recv_data_from_workers(int rank, int *data){
	int received = 0;
	for(int i = 0; i < num_neigh_worker[rank]; i++){
		int recv_size;
		recv_data_in_buffer(rank, neigh_worker[rank][i], &recv_size, data + received);
		received += recv_size;
	}

	return received;
}

/**
 * @brief aplica o operatie (inmultire cu 5) pe vectorul n, n-1, ..., 1
 * in mod distribuit
 * 
 * @param rank id-ul procesului curnt
 * @param size dimensiunea vectorului
 */
void calculate(int rank, int size){
	if(rank < COORD_NUM){ // coordinator
		if(rank == ROOT){ // procesul 0 initializeaza vectorul si il afiseaza

			int *data;
			data = calloc(size, sizeof(int));
			for(int i = 0; i < size; i++){
				data[i] = size - (i + 1);
			}
			
		
			int left_worker_count = 0; // nr de procese muncitor la stanga lui 0
			int right_worker_count = 0; // nr de procese muncitor la dreapta lui 0

			for(int k = neigh_coord_left[rank]; k != -1; k = neigh_coord_left[k]){
				if(parent[k] != neigh_coord_right[k]) 
					break;
				left_worker_count += num_neigh_worker[k];
			}
			for(int k = neigh_coord_right[rank]; k != -1; k = neigh_coord_right[k]){
				if(parent[k] != neigh_coord_left[k]) 
					break;
				right_worker_count += num_neigh_worker[k];
			}
			int right; // nr de valori de trimis la dreapta
			int left; // nr de valori de trimis la stanga
			int down; // nr de valori de trimis in jos(la muncitorii din clusterul lui 0)
			int aux;
			proportional_partition(size, num_neigh_worker[rank], right_worker_count + left_worker_count, &down, &aux);
			proportional_partition(aux, right_worker_count, left_worker_count, &right, &left);
			
			// trimitere valori la muncitori
			send_data_to_workers(rank, down, data);
			if(neigh_coord_right[rank] != -1){ // trimitere valori la dreapta(daca se poate)
				send_int(rank, neigh_coord_right[rank], right_worker_count);
				send_data(rank, neigh_coord_right[rank], right, data + down);
				
			}
			if(neigh_coord_left[rank] != -1){ // trimitere valori la stanga(daca se poate)
				send_int(rank, neigh_coord_left[rank], left_worker_count);
				send_data(rank, neigh_coord_left[rank], left, data + down + right);
			}
			// primire valori de la muncitori
			down = recv_data_from_workers(rank, data);
			if(neigh_coord_right[rank] != -1) // primire valori de la dreapta(daca se poate)
				recv_data_in_buffer(rank, neigh_coord_right[rank], &right, data + down);
			if(neigh_coord_left[rank] != -1) // primire valori de la stanga(daca se poate)
				recv_data_in_buffer(rank, neigh_coord_left[rank], &left, data + down + right);

			// afisare rezultat
			printf("Rezultat:");
			for(int i = 0; i < size; i++)
				printf(" %d", data[i]);
			printf("\n");
		}
		else {
			// ceilalti coordonatori
			if(parent[rank] == -1){
				// eliminare proces izolat(nu este conectat la 0)
				send_data_to_workers(rank, 0, NULL); // muncitorii se asteapta la un mesaj cu valori
				recv_data_from_workers(rank, NULL); 
				return; 
			} 
			int workers_remaining;
			// primire nr de muncitori ramasi
			recv_int(rank, parent[rank], &workers_remaining);
			int *data, size;
			recv_data_dynamic(rank, parent[rank], &size, &data);
			
			// trimitere date la municitori

			int chunk_size = size / workers_remaining;
			int kept_data_size; // numarul de valori pastrate de proces; restul sunt trimise procesului urmator
			int passed_data_size;
			proportional_partition(size, num_neigh_worker[rank] , workers_remaining - num_neigh_worker[rank], &kept_data_size, &passed_data_size);
			
			send_data_to_workers(rank, kept_data_size, data);
			
			if(workers_remaining != num_neigh_worker[rank]){ 
				// nu am ajuns la capat, deci trebuie sa trimitem restul valorilor in continuare
				

				int next_process; //id-ul procesului urmator
				if(parent[rank] == neigh_coord_left[rank]) 
					next_process = neigh_coord_right[rank];
				else 
					next_process = neigh_coord_left[rank];
				send_int(rank, next_process, workers_remaining - num_neigh_worker[rank]); // numarul de muncitori ramasi in continuare
				send_data(rank, next_process, passed_data_size, data + kept_data_size); // valorile neprocesate

				
				// primire date de la procesul urmator
				int s;
				recv_data_in_buffer(rank, next_process, &s, data + kept_data_size);

			}
			// primire date de la muncitori
			recv_data_from_workers(rank, data);
			// trimitere inapoi la parinte
			send_data(rank, parent[rank], size, data);

		}
	}else{ 
		// muncitorii primesc un vector, aplica o operatie pe el (inmultire cu 5)
		// si il trimit inapoi
		int *data, size;
		int src = recv_data_dynamic(rank, MPI_ANY_SOURCE, &size, &data);
		for(int i = 0; i < size; i++){
			data[i] = op(data[i]); // aplicare operatie
		}
		send_data(rank, src, size, data);
		free(data);
	}
	
}

int main(int argc, char * argv[]) {
	int rank, nProcesses;
	int vector_len, top_error;
	if(argc < 3) {
		printf("Usage: ./tema3 <dim_vec> <eroare_comunicatie>");
	}
	vector_len = atoi(argv[1]);
	top_error = atoi(argv[2]);

	MPI_Init(&argc, &argv);

	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &nProcesses);

	init(rank, top_error);
	distribute_topology(rank);
	print_topology(rank);
	calculate(rank, vector_len);

	MPI_Finalize();
	return 0;
}