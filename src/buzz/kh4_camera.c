#define _GNU_SOURCE

#include <stdio.h>
#include <signal.h>
#include <limits.h>
#include "kh4_utility.h"


    int SELECTED_COLOR[] = {51,60,65}; // color detection R, G, B
	int DETECTION_THRESHOLD = 30; // color detection threshold
	int DETECTION_THRESHOLD1 = 28;
	int ret,x,y,i,quitr=0,r,g,b,t;
	int center[4];
	unsigned int dWidth=296;	// image width (max 752)
	unsigned int dHeight=196; // image height  (max 480)
	
	unsigned char* buffer=NULL; // pointer for image buffer
	unsigned char* output_buff=NULL; // pointer for image buffer


void initialize_camera(){
	// camera initialisation
		if ((ret=kb_camera_init(&dWidth, &dHeight))<0)
		{
			fprintf(stderr,"camera init error %d\r\n",ret);
			free(buffer);
			//return -1;
		} else
		{
			switch(ret) {
				case 1:
					printf("width adjusted to %d\r\n",dWidth);
					break;
				case 2:
					printf("height adjusted to %d\r\n",dHeight);
					break;
				case 3:
					printf("width adjusted to %d and height adjusted to %d !\r\n",dWidth,dHeight);
					break;
				default:
					break;
			}
		}
		// allocating memory for image
		//printf("Initialiseing buffer\n");
		buffer=malloc(dWidth*dHeight*3*sizeof(char));
		if (buffer==NULL)
		{
			fprintf(stderr,"could alloc image buffer!\r\n");
			free(buffer);
			//return -2;
		}
		//initailize multi frame mode
		if(kb_captureStart()<0)
	  	{
			free(buffer);
			kb_camera_release();
			fprintf(stderr,"ERROR: capture start error in mutli frames!\r\n");
	   		// return -3;
  		}
  		//printf("started mutli- frame camera");
  		// allocate memory for output
		output_buff=malloc(dWidth*dHeight*sizeof(char));
		
	

}
int find_colour(int min, int max, unsigned char* out){
	int i=0;
    int blue=0;
    int green=0;
for(i=min;i<max;i++){
if(out[i]==1){
 green++;
}
else if (out[i]==2){
 blue++;	
}
}
//printf("blue : %d green: %d \n",blue, green);
i = 0;
if(green > blue){
	i=1;
}
else if(blue > green ){
    i=2;	
}
return i;
}
int* get_blob_pos(){

		
	
		//int maxBlobMass  =50000;
  		//int minBlobMass  =100;
  		t = clock();
  		int out_counter=0;
  		//printf("before obatining image\n");
		  if ((ret=kb_frameRead(buffer))<0)
	  	{
	  		fprintf(stderr,"ERROR: frame capture error %d!\r\n",ret);
	  	} else
	  	{
	  	  //printf("obatined image\n");
	  	  t = clock() - t;
		  // find positions of pixels having a similar color	
	  	  
	  	    	  //printf("camera Processing\n");
	  	  		//printf("pixel R : %d ,  G : %d   ,  B  : %d \n",buffer[100+100*dWidth],buffer[100+100*dWidth+1],buffer[100+100*dWidth+2] );
				for (y=0; y<dHeight;y++)
				{
					for (x=0; x<dWidth;x++)
					{
						i=3*(x+y*dWidth); // compute array index
						out_counter=x+y*dWidth;
						ret=0;
					
						
						if ( (buffer[i+1]> buffer[i]*(1+DETECTION_THRESHOLD/100.0)) && (buffer[i+1]> buffer[i+2]*(1+DETECTION_THRESHOLD/100.0)) )	
							ret=1;
							//printf("green\n");
						
					    
						
							// if in R range
							if ( ((buffer[i]>(1-DETECTION_THRESHOLD/100.0)*SELECTED_COLOR[0]) &&  (buffer[i]<(1+DETECTION_THRESHOLD/100.0)*SELECTED_COLOR[0]))
						   // and in G range
						  && ((buffer[i+1]>(1-DETECTION_THRESHOLD/100.0)*SELECTED_COLOR[1]) &&  (buffer[i+1]<(1+DETECTION_THRESHOLD/100.0)*SELECTED_COLOR[1]))
							// and in B range
							 && ((buffer[i+2]>(1-DETECTION_THRESHOLD/100.0)*SELECTED_COLOR[2]) &&  (buffer[i+2]<(1+DETECTION_THRESHOLD/100.0)*SELECTED_COLOR[2])) )
							 	ret=2;
						
							
		 
						if (ret==1) 
						{	
							output_buff[out_counter]=1; //foreground
							//output_buff[i]=buffer[i];
							//output_buff[i+1]=buffer[i+1];  	 // forground 
							//output_buff[i+2]=buffer[i+2]; 
							
						} 
						else if (ret ==2) {
							output_buff[out_counter]=2;
							//printf("blue\n");
						}
						else
						{
							//output_buff[i]=255;
							//output_buff[i+1]=255;  	 // forground 
							//output_buff[i+2]=255;  	 // forground 

							output_buff[out_counter]=0;  	 // background
							
						}
						
					} // for x			
				} // for y
				//out_counter=0;
				// This is the neighbouring pixel pattern. For position X, A, B, C & D are checked
				// A B C
				// D X
				
				int srcPtr = 0;
			int aPtr = -dWidth - 1;
			int bPtr = -dWidth;
			int cPtr = -dWidth + 1;
			int dPtr = -1;
			int label = 1;
			int tableSize = dWidth * dHeight / 4;
			int labelBuffer [dWidth * dHeight];
			int labelTable [tableSize];
			int xMinTable [tableSize];
			int xMaxTable [tableSize];
			int yMinTable [tableSize];
			int yMaxTable [tableSize];
			int massTable [tableSize];
			for (y=0; y<dHeight;y++) //start from second row
			{
				
				for (x=0; x<dWidth;x++) //start from second column and ignore last column
				{
					labelBuffer[srcPtr] = 0;
					if (output_buff[srcPtr] == 1 || output_buff[srcPtr] == 2 )
					{
						//printf("inside check\n");
						//out_counter=x+y*dWidth;
						// Find label for neighbours (0 if out of range)
						int aLabel = (x > 0 && y > 0)			? labelTable[labelBuffer[aPtr]] : 0;
						int bLabel = (y > 0)						? labelTable[labelBuffer[bPtr]] : 0;
						int cLabel = (x < dWidth-1 && y > 0)	? labelTable[labelBuffer[cPtr]] : 0;
						int dLabel = (x > 0)						? labelTable[labelBuffer[dPtr]] : 0;

						int min =INT_MAX;
						if (aLabel != 0 && aLabel < min) min = aLabel;
						if (bLabel != 0 && bLabel < min) min = bLabel;
						if (cLabel != 0 && cLabel < min) min = cLabel;
						if (dLabel != 0 && dLabel < min) min = dLabel;
						// If no neighbours in foreground
						if (min == INT_MAX)
						{
							labelBuffer[srcPtr] = label;
							labelTable[label] = label;

							// Initialise min/max x,y for label
							yMinTable[label] = y;
							yMaxTable[label] = y;
							xMinTable[label] = x;
							xMaxTable[label] = x;
							massTable[label] = 1;

							label ++;
						}
						// Neighbour found
						else
						{
							// Label pixel with lowest label from neighbours
							labelBuffer[srcPtr] = min;

							// Update min/max x,y for label
							yMaxTable[min] = y;
							massTable[min]++;
							if (x < xMinTable[min]) xMinTable[min] = x;
							if (x > xMaxTable[min]) xMaxTable[min] = x;

							if (aLabel != 0) labelTable[aLabel] = min;
							if (bLabel != 0) labelTable[bLabel] = min;
							if (cLabel != 0) labelTable[cLabel] = min;
							if (dLabel != 0) labelTable[dLabel] = min;
						}

					} // if foreground
					srcPtr ++;
					aPtr ++;
					bPtr ++;
					cPtr ++;
					dPtr ++;
					//printf("src ptr %d, a ptr %d , b ptr %d , c Ptr %d , d ptr %d\n",srcPtr,aPtr,bPtr,cPtr,dPtr);
				} // for x			
			} // for y

			int max_blobsize=0;
			int max_blobnumber=0;
			//printf("maximum blob size %d\n",max_blobsize);
			for (i=label-1 ; i>0 ; i--)
			{
				if (labelTable[i] != i)
				{
					if (xMaxTable[i] > xMaxTable[labelTable[i]]) xMaxTable[labelTable[i]] = xMaxTable[i];
					if (xMinTable[i] < xMinTable[labelTable[i]]) xMinTable[labelTable[i]] = xMinTable[i];
					if (yMaxTable[i] > yMaxTable[labelTable[i]]) yMaxTable[labelTable[i]] = yMaxTable[i];
					if (yMinTable[i] < yMinTable[labelTable[i]]) yMinTable[labelTable[i]] = yMinTable[i];
					massTable[labelTable[i]] += massTable[i];

					int l = i;
					while (l != labelTable[l]) l = labelTable[l];
					labelTable[i] = l;
				}
				else
				{
					// Ignore blobs that butt against corners
					if (i == labelBuffer[0]);									// Top Left
					if (i == labelBuffer[dWidth]) ;								// Top Right
					if (i == labelBuffer[(dWidth*dHeight) - dWidth + 1]) ;	// Bottom Left
					if (i == labelBuffer[(dWidth*dHeight) - 1]) ;			// Bottom Right

					/*if (massTable[i] >= minBlobMass && (massTable[i] <= maxBlobMass || maxBlobMass == -1))
					{
						//printf("Blob lable: %d , blob x min: %d , blob x max: %d , blob y min : %d, blob y max : %d, blob mass : %d",i,xMinTable[i], xMaxTable[i], yMinTable[i], yMaxTable[i], massTable[i]);
						//Blob blob = new Blob(xMinTable[i], xMaxTable[i], yMinTable[i], yMaxTable[i], massTable[i]);
						//blobList.add(blob);
					}*/
					//printf("I am before check\n");
					//printf("mass table : %d \n, max blob size : %d \n",massTable[i],max_blobsize);

					if(massTable[i] >= max_blobsize){
						max_blobsize=massTable[i];
						max_blobnumber=i;
						//printf("I am here putting new value\n" );
					}
				}
			}//for end
			//printf("biggest blob number is: %d \n, biggest blob size: %d \n, blob x min pos: %d \n, blob x max pos: %d \n, blob y min pos: %d \n, blob y max pos :%d \n",max_blobnumber,max_blobsize,xMinTable[max_blobnumber], xMaxTable[max_blobnumber], yMinTable[max_blobnumber], yMaxTable[max_blobnumber]); 
			center[0] =(xMaxTable[max_blobnumber]-xMinTable[max_blobnumber])/2;
			center[1] =(yMaxTable[max_blobnumber]-yMinTable[max_blobnumber])/2;
			center[0]=xMinTable[max_blobnumber]+center[0];
			center[1]=yMinTable[max_blobnumber]+center[1];
			center[2]=max_blobsize;

			center[3]=find_colour(xMinTable[max_blobnumber]+yMinTable[max_blobnumber]*dWidth, xMaxTable[max_blobnumber]+yMaxTable[max_blobnumber]*dWidth, output_buff);
			printf("center for x:  %d , center for y: %d , blob size : %d, blob colour : %d \n",center[0],center[1], center[2], center[3]);
			//printf("R : %d G  : %d B : %d \n",buffer[(center[0]+center[1])*3],buffer[(center[0]+center[1]+1)*3],buffer[(center[0]+center[1]+2)*3]);
			for ( i=label-1 ; i>0 ; i--)
			{
				if (labelTable[i] != i)
				{
					int l = i;
					while (l != labelTable[l]) l = labelTable[l];
					labelTable[i] = l;
				}
			}

		}
	
	return center;
}

void stop_camera(){
	free(buffer);
	free(output_buff);
	if (kb_captureStop()<0)
	{
		fprintf(stderr,"ERROR: capture stop error in mutli frames!\r\n");
	}
	// releasing camera
	kb_camera_release();
}

