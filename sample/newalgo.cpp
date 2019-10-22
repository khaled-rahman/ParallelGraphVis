#include "newalgo.h"
#include "nblas.h"

	newalgo::newalgo(CSR<INDEXTYPE, VALUETYPE> &A_csr, string input, string outputd, int init, double weight, double th, string ifile){
		graph.make_empty();
		graph = A_csr;
		outputdir = outputd;
		initfile = ifile;
		//cout << initfile << endl;
		W = weight;
		filename = input;
		threshold = th;
		nCoordinates = static_cast<Coordinate<VALUETYPE> *> (::operator new (sizeof(Coordinate<VALUETYPE>[A_csr.rows])));
		blasX = static_cast<VALUETYPE *> (::operator new (sizeof(VALUETYPE[A_csr.rows])));
                blasY = static_cast<VALUETYPE *> (::operator new (sizeof(VALUETYPE[A_csr.rows])));
		this->init = init;
	}
	void newalgo::randInit(){
		#pragma omp parallel for schedule(static)
		for(INDEXTYPE i = 0; i < graph.rows; i++){
			VALUETYPE x, y;
			do{
				x = -1.0 + 2.0 * rand()/(RAND_MAX+1.0);
				y = -1.0 + 2.0 * rand()/(RAND_MAX+1.0);
			}while(x * x + y * y > 1.0);
			x = x * MAXMIN;
			y = y * MAXMIN;
			nCoordinates[i] = Coordinate <VALUETYPE>(x, y, i);
		}
	}
	void newalgo::initDFS(){
                int visited[graph.rows] = {0};
                stack <int> STACK;
		double scalefactor = 1.0;
		minX = minY = 1.0;//numeric_limits<double>::max();
		maxX = maxY = 1.0;//numeric_limits<double>::min();
                STACK.push(0);
		double radi = 0.1;
                visited[0] = 1;
                nCoordinates[0] = Coordinate <VALUETYPE>(1.0, 1.0);
                while(!STACK.empty()){
                        int parent = STACK.top();
                        STACK.pop();
                        if(parent < graph.rows - 1 && (graph.rowptr[parent+1] - graph.rowptr[parent]) > 0){
                                double deg = 360.0 / (graph.rowptr[parent+1] - graph.rowptr[parent]);
                                double degree = 0;
                                for(INDEXTYPE n = graph.rowptr[parent]; n < graph.rowptr[parent+1]; n++){
                                        if(visited[graph.colids[n]] == 0){
                                                nCoordinates[graph.colids[n]] = Coordinate <VALUETYPE>(nCoordinates[parent].getX() + radi*cos(PI*(degree)/180.0), nCoordinates[parent].getY() + radi*sin(PI*(degree)/180.0)) + static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
						visited[graph.colids[n]] = 1;
                                                STACK.push(graph.colids[n]);
                                                degree += deg;
						if(minX > nCoordinates[graph.colids[n]].getX()){
							minX = nCoordinates[graph.colids[n]].getX();
						}
						else if(maxX < nCoordinates[graph.colids[n]].getX()){
                                                        maxX = nCoordinates[graph.colids[n]].getX();
                                                }
						if(minY > nCoordinates[graph.colids[n]].getY()){
                                                        minY = nCoordinates[graph.colids[n]].getY();
                                                }
                                                else if(maxY < nCoordinates[graph.colids[n]].getY()){
                                                        maxY = nCoordinates[graph.colids[n]].getY();
                                                }
                                        }
                                }
                        }else{
				if((graph.nnz - graph.rowptr[parent]) <= 0) continue;

                                double deg = 360.0 / (graph.nnz - graph.rowptr[parent]);
                                double degree = 0;
                                for(INDEXTYPE n = graph.rowptr[parent]; n < graph.nnz; n++){
                                        if(visited[graph.colids[n]] == 0){
                                                nCoordinates[graph.colids[n]] = Coordinate <VALUETYPE>(nCoordinates[parent].getX() + radi*cos(PI*(degree)/180.0), nCoordinates[parent].getY() + radi*sin(PI*(degree)/180.0)) + static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
						visited[graph.colids[n]] = 1;
                                                STACK.push(graph.colids[n]);
                                                degree += deg;
						if(minX > nCoordinates[graph.colids[n]].getX()){
                                                        minX = nCoordinates[graph.colids[n]].getX();
                                                }
                                                else if(maxX < nCoordinates[graph.colids[n]].getX()){
                                                        maxX = nCoordinates[graph.colids[n]].getX();
                                                }
                                                if(minY > nCoordinates[graph.colids[n]].getY()){
                                                        minY = nCoordinates[graph.colids[n]].getY();
                                                }
                                                else if(maxY < nCoordinates[graph.colids[n]].getY()){
                                                        maxY = nCoordinates[graph.colids[n]].getY();
                                                }
                                        }
                                }
                        }
                }
		scalefactor = 2.0 * MAXMIN / max(maxX - minX, maxY - minY);
		#pragma omp parallel for schedule(static)
		for(int i = 0; i < graph.rows; i++){
			nCoordinates[i] = nCoordinates[i] * scalefactor;
			blasX[i] = nCoordinates[i].x;
                        blasY[i] = nCoordinates[i].y;
		}
        }

	vector<VALUETYPE> newalgo::batchlayout(INDEXTYPE ITERATIONS, INDEXTYPE NUMOFTHREADS, INDEXTYPE BATCHSIZE){
		INDEXTYPE LOOP = 0;
                VALUETYPE start, end, ENERGY;
                VALUETYPE STEP = 1.0;
                vector<VALUETYPE> result;
		VALUETYPE (newalgo::*frm)(Coordinate<VALUETYPE>, Coordinate<VALUETYPE>);
		frm = &newalgo::frmodel;
                ENERGY = numeric_limits<VALUETYPE>::max();
                omp_set_num_threads(NUMOFTHREADS);
                start = omp_get_wtime();
                initDFS();
		NBLAS(BATCHSIZE, ITERATIONS, NUMOFTHREADS, nCoordinates, &ENERGY, graph, frm);
                end = omp_get_wtime();
                cout << "NBLAS Minibatch Size:" << BATCHSIZE  << endl;
                cout << "NBLAS Minbatch Energy:" << ENERGY << endl;
                cout << "NBLAS Minibatch Parallel Wall time required:" << end - start << endl;
                writeToFile("NBLAS"+ to_string(BATCHSIZE)+"PARAOUT" + to_string(LOOP));
                result.push_back(ENERGY);
                result.push_back(end - start);
		return result;
	}
	
	vector<VALUETYPE> newalgo::EfficientVersion(INDEXTYPE ITERATIONS, INDEXTYPE NUMOFTHREADS, INDEXTYPE BATCHSIZE){
                INDEXTYPE LOOP = 0;
                VALUETYPE start, end, ENERGY, ENERGY0, *pb_X, *pb_Y;
                VALUETYPE STEP = 1.0;
                vector<VALUETYPE> result;
                pb_X = static_cast<VALUETYPE *> (::operator new (sizeof(VALUETYPE[BATCHSIZE])));
                pb_Y = static_cast<VALUETYPE *> (::operator new (sizeof(VALUETYPE[BATCHSIZE])));
                ENERGY0 = ENERGY = numeric_limits<VALUETYPE>::max();
                omp_set_num_threads(NUMOFTHREADS);
                start = omp_get_wtime();
                initDFS();
                while(LOOP < ITERATIONS){
                        ENERGY0 = ENERGY;
                        ENERGY = 0;
                        for(int i = 0; i < BATCHSIZE; i++){
                                pb_X[i] = pb_Y[i] = 0;
                        }
		
		//TODO: unrolling and jamming
		// no reverse

                        for(INDEXTYPE b = 0; b < (graph.rows / BATCHSIZE); b += 1){
                                #pragma omp parallel for schedule(static)
                                for(INDEXTYPE i = b * BATCHSIZE; i < (b + 1) * BATCHSIZE; i += 1){
                                        VALUETYPE fx = 0, fy = 0, distX, distY, dist, dist2;
                                        int ind = i-b*BATCHSIZE;
                                        for(INDEXTYPE j = 0; j < i; j += 1){
                                                distX = blasX[j] - blasX[i];
                                                distY = blasY[j] - blasY[i];
                                                dist2 = 1.0 / (distX * distX + distY * distY);
                                                fx += distX * dist2;
                                                fy += distY * dist2;
                                        }
					for(INDEXTYPE j = i+1; j < graph.rows; j += 1){
                                                distX = blasX[j] - blasX[i];
                                                distY = blasY[j] - blasY[i];
                                                dist2 = 1.0 / (distX * distX + distY * distY);
                                                fx += distX * dist2;
                                                fy += distY * dist2;
                                        }
                                        for(INDEXTYPE j = graph.rowptr[i]; j < graph.rowptr[i+1]; j += 1){
                                                int v = graph.colids[j];
                                                distX = blasX[v] - blasX[i];
                                                distY = blasY[v] - blasY[i];
                                                dist2 = 1.0 / (distX * distX + distY * distY);
                                                dist = sqrt(distX * distX + distY * distY);
                                                pb_X[ind] += distX * dist + distX * dist2;
                                                pb_Y[ind] += distY * dist + distY * dist2;
                                        }
                                        pb_X[ind] = pb_X[ind] - fx;
                                        pb_Y[ind] = pb_Y[ind] - fy;
                                }
                                for(INDEXTYPE i = b * BATCHSIZE; i < (b + 1) * BATCHSIZE; i++){
                                        int ind = i-b*BATCHSIZE;
                                        double dist = 1.0 / sqrt(pb_X[ind]*pb_X[ind] + pb_Y[ind]*pb_Y[ind]);
                                        blasX[i] += pb_X[ind] * STEP * dist;
                                        blasY[i] += pb_Y[ind] * STEP * dist;
                                        ENERGY += (pb_X[ind] * pb_X[ind] + pb_Y[ind] * pb_Y[ind]);
                                }
                        }
			//clean up loop
			INDEXTYPE cleanup = (graph.rows/BATCHSIZE) * BATCHSIZE;
			#pragma omp parallel for schedule(static)
			for(INDEXTYPE i = cleanup; i < graph.rows; i += 1){
				INDEXTYPE ind = i- cleanup;
                		VALUETYPE fx = 0, fy = 0, distX, distY, dist, dist2;
				for(INDEXTYPE j = graph.rowptr[i]; j < graph.rowptr[i+1]; j += 1){
					int v = graph.colids[j];
                                        distX = blasX[v] - blasX[i];
                                        distY = blasY[v] - blasY[i];
                                        dist2 = 1.0 / (distX * distX + distY * distY);
                                        dist = sqrt(distX * distX + distY * distY);
                                        pb_X[ind] += distX * dist + distX * dist2;
                                        pb_Y[ind] += distY * dist + distY * dist2;	
				}
				for(INDEXTYPE j = 0; j < i; j += 1){
                                        distX = blasX[j] - blasX[i];
                                        distY = blasY[j] - blasY[i];
                                        dist2 = 1.0 / (distX * distX + distY * distY);
                                        fx += distX * dist2;
                                        fy += distY * dist2;
                                }
                               	for(INDEXTYPE j = i+1; j < graph.rows; j += 1){
                                        distX = blasX[j] - blasX[i];
                                        distY = blasY[j] - blasY[i];
                                        dist2 = 1.0 / (distX * distX + distY * distY);
                                        fx += distX * dist2;
                                        fy += distY * dist2;
                               	}
				pb_X[ind] = pb_X[ind] - fx;
				pb_Y[ind] = pb_Y[ind] - fy;
			}
			for(INDEXTYPE i = cleanup; i < graph.rows; i += 1){
                        	int ind = i-cleanup;
                                double dist = 1.0 / sqrt(pb_X[ind]*pb_X[ind] + pb_Y[ind]*pb_Y[ind]);
                                blasX[i] += pb_X[ind] * STEP * dist;
                                blasY[i] += pb_Y[ind] * STEP * dist;
                                ENERGY += (pb_X[ind] * pb_X[ind] + pb_Y[ind] * pb_Y[ind]);
                       	}
                        STEP = STEP * 0.999;
                        LOOP++;
                }
		end = omp_get_wtime();
                cout << "Efficient Minibatch Size:" << BATCHSIZE  << endl;
                cout << "Efficient Minbatch Energy:" << ENERGY << endl;
                cout << "Efficient Minibatch Parallel Wall time required:" << end - start << endl;
                writeToFile("Efficient"+ to_string(BATCHSIZE)+"PARAOUT" + to_string(LOOP));
                result.push_back(ENERGY);
                result.push_back(end - start);
                return result;
        }
	
	void newalgo::print(){
		for(INDEXTYPE i = 0; i < graph.rows; i++){
                	cout << "Node:" << i << ", X:" << nCoordinates[i].getX() << ", Y:" << nCoordinates[i].getY()<< endl;
        	}
		cout << endl;
	}
	void newalgo::writeToFile(string f){
		stringstream  data(filename);
    		string lasttok;
    		while(getline(data,lasttok,'/'));
		filename = outputdir + lasttok + f + ".txt";
		ofstream output;
		output.open(filename);
		cout << "Creating output file in following directory:" << filename << endl;
		for(INDEXTYPE i = 0; i < graph.rows; i++){
			output << nCoordinates[i].getX() <<"\t"<< nCoordinates[i].getY() << "\t" << i+1 << endl;
		}
		output.close();
	}
