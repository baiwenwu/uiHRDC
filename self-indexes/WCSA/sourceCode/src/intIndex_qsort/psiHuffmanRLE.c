#include "psiHuffmanRLE.h"

// IMPLEMENTACION DAS FUNCIONS

void destroyHuffmanCompressedPsi(HuffmanCompressedPsi *compressedPsi) {
	freeHuff(compressedPsi->diffsHT);
	free(compressedPsi->samples);
	free(compressedPsi->samplePointers);
	free (compressedPsi->stream);
}

HuffmanCompressedPsi huffmanCompressPsi(unsigned int *Psi, unsigned int psiSize, unsigned int T, unsigned int nS) {
	
	HuffmanCompressedPsi cPsi;
	
	int absolute_value;
	register unsigned int index, ptr, samplesPtr, samplePointersPtr;
	unsigned int runLenght, binaryLenght;
	
	int *diffs;	
	unsigned int *huffmanDst;
	
	// Estructuras da funcion comprimida (para logo asignar)
	// Tam�n se podian almacenar directamente
	THuff diffsHT;
	unsigned int numberOfSamples;
	unsigned int *samples;
	unsigned int sampleSize;
	unsigned int *samplePointers;
	unsigned int pointerSize;
	unsigned int *stream;
	unsigned int streamSize;
	
	// Variables que marcan os intervalos dentro do vector de frecuencias
	unsigned int runLenghtStart = nS - 64 - T; 	// Inicio das Runs
	unsigned int negStart = nS - 64;			// Inicio dos Negativos
	unsigned int bigStart = nS - 32;			// Inicio dos Grandes (>runLenghtStart)
	
	// Para estadistica
	unsigned int totalSize;
	
	// Reservamos espacio para a distribuci�n de valores de Psi
	huffmanDst = (unsigned int *)malloc(sizeof(int)*nS);
	for(index=0;index<nS;index++) huffmanDst[index]=0;
	
	// Inicializamos diferencias	
	diffs = (int *)malloc(sizeof(int)*psiSize);	
	diffs[0] = 0;
	for(index=1; index<psiSize; index++) 
		diffs[index] = Psi[index] - Psi[index-1];
	
	// Calculamos a distribucion de frecuencias
	runLenght = 0;
	for(index=0; index<psiSize; index++) {
		
		if(index%T) {
			
			if(diffs[index]==1) {
				runLenght++;
			} else {	// Non estamos nun run
				if(runLenght) {
					huffmanDst[runLenght+runLenghtStart]++;
					runLenght = 0;
				}
				if(diffs[index]>1 && diffs[index]<runLenghtStart) 
					huffmanDst[diffs[index]]++;
				else
					if(diffs[index]<0) {	// Valor negativo
						absolute_value = -diffs[index];
						binaryLenght = bits(absolute_value);
						huffmanDst[binaryLenght+negStart-1]++;
					} else {				// Valor grande >= 128
						absolute_value = diffs[index];
						binaryLenght = bits(absolute_value);
						huffmanDst[binaryLenght+bigStart-1]++;
					}
			}
			
		} else { // Rompemos o run porque atopamos unha mostra
			if(runLenght) {
				huffmanDst[runLenght+runLenghtStart]++;
				runLenght = 0;
			}
		}
		
	}
	
	if(runLenght) huffmanDst[runLenght+runLenghtStart]++;
	
	// Creamos o arbol de Huffman
	diffsHT = createHuff(huffmanDst,nS-1,UNSORTED);
	
	// Calculamos o espacio total ocupado pola secuencia Huffman + RLE
	streamSize = diffsHT.total;
	for(index=negStart;index<bigStart;index++) 
		streamSize += huffmanDst[index]*(index-negStart+1);	// Negativos
	for(index=bigStart;index<nS;index++) 
		streamSize += huffmanDst[index]*(index-bigStart+1);		// Grandes	
	
	// Calculamos o numero de mostras e o espacio ocupado por cada mostra e por cada punteiro
	numberOfSamples = (psiSize+T-1)/T;	
	sampleSize = bits(psiSize);
	pointerSize = bits(streamSize);	

	// Reservamos espacio para a secuencia e para as mostras e punteiros
	samples = (unsigned int *)malloc(sizeof(uint)*((numberOfSamples*sampleSize+31)/32));	
		samples[((numberOfSamples*sampleSize+31)/32)-1] =0000; //initialized only to avoid valgrind warnings
	samplePointers = (unsigned int *)malloc(sizeof(int)*((numberOfSamples*pointerSize+31)/32));
		samplePointers[((numberOfSamples*pointerSize+31)/32)-1] = 0000;  //initialized only to avoid valgrind warnings
	stream = (unsigned int *)malloc(sizeof(int)*((streamSize+31)/32));
		stream[((streamSize+31)/32)-1]=0000;//initialized only to avoid valgrind warnings
	
	// Comprimimos secuencialmente (haber� que levar un punteiro desde o inicio)
	ptr = 0;
	samplesPtr = 0;
	samplePointersPtr = 0;
	runLenght = 0;
	for(index=0; index<psiSize; index++) {
		
		if(index%T) {
			
			if(diffs[index]==1) {
				runLenght++;
			} else {	// Non estamos nun run
				if(runLenght) {
					ptr = encodeHuff(diffsHT,runLenght+runLenghtStart,stream,ptr);
					runLenght = 0;
				}
				if(diffs[index]>1 && diffs[index]<runLenghtStart) {				
					ptr = encodeHuff(diffsHT,diffs[index],stream,ptr);	
				}	
				else
					if(diffs[index]<0) {	// Valor negativo
						absolute_value = -diffs[index];
						binaryLenght = bits(absolute_value);
						ptr = encodeHuff(diffsHT,binaryLenght+negStart-1,stream,ptr);
						bitwrite(stream,ptr,binaryLenght,absolute_value);
						ptr += binaryLenght;						
					} else {				// Valor grande >= 128
						absolute_value = diffs[index];
						binaryLenght = bits(absolute_value);					
						ptr = encodeHuff(diffsHT,binaryLenght+bigStart-1,stream,ptr);
						bitwrite(stream,ptr,binaryLenght,absolute_value);						
						ptr += binaryLenght;
					}
			}
			
		} else { // Rompemos o run porque atopamos unha mostra
			if(runLenght) {				
				ptr = encodeHuff(diffsHT,runLenght+runLenghtStart,stream,ptr);
				runLenght = 0;
			}
			bitwrite(samples,samplesPtr,sampleSize,Psi[index]);
			samplesPtr += sampleSize;
			bitwrite(samplePointers,samplePointersPtr,pointerSize,ptr);
			samplePointersPtr += pointerSize;
		}
		
	}
	
	if(runLenght) {	
		ptr = encodeHuff(diffsHT,runLenght+runLenghtStart,stream,ptr);
	}
	
	// Amosamos o espacio ocupado
	totalSize = sizeof(HuffmanCompressedPsi) + 
		sizeof(int)*((numberOfSamples*sampleSize+31)/32) + 
		sizeof(int)*((numberOfSamples*pointerSize+31)/32) +
		sizeof(int)*((streamSize+31)/32) + sizeHuff(diffsHT);

	printf("\n\t Compressed Psi size = %d bytes, with %d different symbols.", totalSize, nS);
	
	// Necesario antes de decodificar
	prepareToDecode(&diffsHT);
	
	// Asignamos os valores a cPsi e devolvemolo
	cPsi.T = T;
	cPsi.diffsHT = diffsHT;
	cPsi.nS = nS;
	cPsi.numberOfSamples = numberOfSamples;
	cPsi.samples = samples;
	cPsi.sampleSize = sampleSize;
	cPsi.samplePointers = samplePointers;
	cPsi.pointerSize = pointerSize;
	cPsi.stream = stream;
	cPsi.streamSize = streamSize;
	cPsi.totalMem = totalSize;
	
	//frees resources not needed in advance
	free(diffs);
	free(huffmanDst);
	
	//returns the data structure that holds the compressed psi.
	return cPsi;	
}


unsigned int getHuffmanPsiValue(HuffmanCompressedPsi *cPsi, unsigned int position) {
	
	register unsigned int index;
	unsigned int sampleIndex, ptr, psiValue, huffmanCode, positionsSinceSample; 
	unsigned int absolute_value, binaryLenght, runLenght;
	
	unsigned int runLenghtStart = cPsi->nS - 64 - cPsi->T;
	unsigned int negStart = cPsi->nS - 64;
	unsigned int bigStart = cPsi->nS - 32;	
	
	sampleIndex = position / cPsi->T;
	psiValue = bitread(cPsi->samples,sampleIndex*cPsi->sampleSize,cPsi->sampleSize);
	ptr = bitread(cPsi->samplePointers,sampleIndex*cPsi->pointerSize,cPsi->pointerSize);
	
	positionsSinceSample = position%cPsi->T;
	
	for(index=0;index<positionsSinceSample;index++) {
		
		ptr = decodeHuff(&cPsi->diffsHT,&huffmanCode,cPsi->stream,ptr);
		
		if(huffmanCode < runLenghtStart) { 	// Incremento directo
			psiValue += huffmanCode;
		}	
		else 
			if(huffmanCode < negStart) {	// Estamos nun run
				runLenght = huffmanCode - runLenghtStart;
				if(index+runLenght>=positionsSinceSample)
					return psiValue+positionsSinceSample-index;
				else {
					psiValue += runLenght;
					index += runLenght-1;
				}
			}
			else
				if(huffmanCode < bigStart) {	// Negativo
					binaryLenght = huffmanCode-negStart+1;
					absolute_value = bitread(cPsi->stream,ptr,binaryLenght);
					ptr += binaryLenght;
					psiValue -= absolute_value;	
				}
				else {	// Grande
					binaryLenght = huffmanCode-bigStart+1;
					absolute_value = bitread(cPsi->stream,ptr,binaryLenght);
					ptr += binaryLenght;
					psiValue += absolute_value;				 
				}
				
	}
	
	return psiValue;

}


void storeHuffmanCompressedPsi(HuffmanCompressedPsi *compressedPsi, char *filename) {

	int file;
	THuff H;

	if( (file = open(filename, O_WRONLY|O_CREAT, 0700)) < 0) {
		printf("Cannot open file %s\n", filename);
		exit(0);
	}
	write(file, &(compressedPsi->T), sizeof(int));
	// Almacenar o arbol de huffman
	H = compressedPsi->diffsHT;
	write(file, &H.max, sizeof(int));
	write(file, &H.lim, sizeof(int));
	write(file, &H.depth, sizeof(int));
//	write(file, H.s.spos, (H.lim+1)*sizeof(int));
	write(file, H.s.symb, (H.lim+1)*sizeof(int));	
	write(file, H.num, (H.depth+1)*sizeof(int));
	write(file, H.fst, (H.depth+1)*sizeof(int));
	// Fin de almacenar o arbol de huffman
	write(file, &(compressedPsi->nS), sizeof(int));
	write(file, &(compressedPsi->numberOfSamples), sizeof(int));
	write(file, &(compressedPsi->sampleSize), sizeof(int));
	write(file,	compressedPsi->samples, ((compressedPsi->numberOfSamples*compressedPsi->sampleSize+31)/32)*sizeof(int));	
	write(file, &(compressedPsi->pointerSize), sizeof(int));
	write(file,	compressedPsi->samplePointers, ((compressedPsi->numberOfSamples*compressedPsi->pointerSize+31)/32)*sizeof(int));
	write(file, &(compressedPsi->streamSize), sizeof(int));
	write(file,	compressedPsi->stream, ((compressedPsi->streamSize+31)/32)*sizeof(int));
	write(file, &(compressedPsi->totalMem), sizeof(int));
	
	close(file);	

}


HuffmanCompressedPsi loadHuffmanCompressedPsi(char *filename) {
	
	HuffmanCompressedPsi compressedPsi;

 	THuff H;
     
	int file;

	if( (file = open(filename, O_RDONLY)) < 0) {
		printf("Cannot read file %s\n", filename);
		exit(0);
	}
	read(file, &(compressedPsi.T), sizeof(int));
	// Cargamos o arbol de Huffman
	read(file, &H.max, sizeof(int));
	read(file, &H.lim, sizeof(int));
	read(file, &H.depth, sizeof(int));
	//H.s.spos = (unsigned int *) malloc((H.lim+1)*sizeof(int));
	//H.s.spos =H.s.symb = (unsigned int *) malloc((H.lim+1)*sizeof(int));
	H.s.symb = (unsigned int *) malloc((H.lim+1)*sizeof(int));
	H.num = (unsigned int *) malloc((H.depth+1)*sizeof(int));	
	H.fst = (unsigned int *) malloc((H.depth+1)*sizeof(int));	

	//read(file, H.s.spos, (H.lim+1)*sizeof(int));
	//fprintf(stderr," \n read %d spos bytes\n", (H.lim+1)*sizeof(int));
	read(file, H.s.symb, (H.lim+1)*sizeof(int));	

	read(file, H.num, (H.depth+1)*sizeof(int));
	read(file, H.fst, (H.depth+1)*sizeof(int));	
	compressedPsi.diffsHT = H;
	// Fin da carga do arbol de Huffman
	read(file, &(compressedPsi.nS), sizeof(int));
	read(file, &(compressedPsi.numberOfSamples), sizeof(int));	
	read(file, &(compressedPsi.sampleSize), sizeof(int));
	compressedPsi.samples = (unsigned int *)malloc(((compressedPsi.numberOfSamples*compressedPsi.sampleSize+31)/32)*sizeof(int));
	read(file, compressedPsi.samples, ((compressedPsi.numberOfSamples*compressedPsi.sampleSize+31)/32)*sizeof(int));
	read(file, &(compressedPsi.pointerSize), sizeof(int));
	compressedPsi.samplePointers = (unsigned int *)malloc(((compressedPsi.numberOfSamples*compressedPsi.pointerSize+31)/32)*sizeof(int));
	read(file, compressedPsi.samplePointers, ((compressedPsi.numberOfSamples*compressedPsi.pointerSize+31)/32)*sizeof(int));
	read(file, &(compressedPsi.streamSize), sizeof(int));
	compressedPsi.stream = (unsigned int *)malloc(((compressedPsi.streamSize+31)/32)*sizeof(int));
	read(file, compressedPsi.stream, ((compressedPsi.streamSize+31)/32)*sizeof(int));
	read(file, &(compressedPsi.totalMem), sizeof(int));
	
	close(file);
		
	return compressedPsi;

}
