
#include "HLS/hls.h"
#include "HLS/ac_int.h"
#include <stdio.h>

#define row 5
#define col 6


hls_avalon_slave_component component 
void sample_buffer(hls_avalon_slave_memory_argument((row*col)*sizeof(uint8)) uint8* pixel_image, ihc::stream_out<uint8>& pixel_out){
    for (int y = 0; y < (row*col); ++y){
            pixel_out.write(pixel_image[y]);
    }
}

hls_always_run_component component
void result_buffer(hls_avalon_slave_memory_argument((row*col)*sizeof(int)) int* result_pixel, ihc::stream_in<int>& result_pixel_stream_in){
    for (int y = 0; y < col*row; ++y){
        result_pixel[y] = result_pixel_stream_in.read();
    }
}

hls_always_run_component component
void integral_image(ihc::stream_in<uint8>& image_pixel_stream_in, ihc::stream_out<int>& integral_value_out){
    int line_sum[row][col] = {0};
    int row_sum = 0;
    uint8 pixel = 0;    
    int col_sum = 0;
   
    for (int i = 0; i< row; i++){
        row_sum = 0;
        for (int j = 0; j < col; j++) {
             // Non-blocking read
             bool success = false;
            pixel = image_pixel_stream_in.tryRead(success);
            if (success) {
                row_sum += pixel;
                line_sum[i][j] = row_sum;
            }
            else{
                line_sum[i][j] = 10;
            }   
        }    
    }
    #pragma unroll
    for (int i = 0; i< col; i++){
        col_sum = 0;
        #pragma unroll
        for (int j = 0; j< row; j++){
            col_sum += line_sum[j][i];
            line_sum[j][i] = col_sum; 
        }
    }

    for (int i = 0; i< row; i++){
        for (int j = 0; j< col; j++){
            integral_value_out.write(line_sum[i][j]);
        }
    }
}

// Testbench

int main (void)
{
    
    int sample_output[row*col];
    int fir_output[row*col];
    int results[row*col];
    int results_tb[row*col];
    uint8 memory_tb[row*col];           // Represents component memory
    int memory_tb2[row*col];           // Represents component memory
    int result[row][col];

    // Samples (8-bit precision)
    uint8 sample[row][col] ={{1,0,0,1,0,0},{0,0,0,1,0,0},{1,0,0,1,1,0},{0,1,1,1,1,0},{0,0,1,0,1,1}};
    int result_expected[row][col] ={{1,1,1,2,2,2},{1,1,1,3,3,3},{2,2,2,5,6,6},{2,3,4,8,10,10},{2,3,5,9,12,13}};
    //int sample2[row][col] ={{2,1,1,2,1,1},{1,1,1,2,1,1},{2,1,1,2,2,1},{1,2,2,2,2,1},{1,1,2,1,2,2}};

    // Streams
    ihc::stream_out<uint8> sample_out;
    ihc::stream_in<uint8> fir_in;
    ihc::stream_out<int> fir_out;
    ihc::stream_in<int> result_in;

    /////// Sample buffer Testbench ///////
    // Load testbench-memory with original image
    for(int y=0 ; y<row ; ++y){
        for(int x=0 ; x<col ; ++x){
            memory_tb[y*col+x] = sample[y][x];
        }
    }
    // sample2
    //for(int y=0 ; y<row ; ++y){
        //for(int x=0 ; x<col ; ++x){
            //memory_tb2[y*col+x] = sample2[y][x];
        //}
    //}
    sample_buffer(memory_tb, sample_out);                           // Call sample-buffer
    for(int y=0 ; y<(row*col) ; ++y){
        sample_output[y]= sample_out.read();
        //printf("%d ", sample_output[y]);                        // Read output-stream
    }    
    /////// Sample buffer Testbench ///////

    /////// FIR-filter Testbench ///////
    for (int i = 0; i < (row*col); ++i){
        fir_in.write(sample_output[i]);                                // Populate input-stream
    }

    for (int i = 0; i < (row*col); ++i){
        ihc_hls_enqueue_noret(&integral_image, fir_in, fir_out);           // Enqueue FIR-filter invocations
    }

    ihc_hls_component_run_all(integral_image);                             // Run all enqueued invocations

    for (int i = 0; i < (row*col); ++i){
        fir_output[i] = fir_out.read();                                // Read output-stream
    }
    /////// FIR-filter Testbench ///////


    /////// Result buffer Testbench ///////
    for (int i = 0; i < (row*col); ++i){
        result_in.write(fir_output[i]);                                // Populate input-stream
    }

    ihc_hls_enqueue_noret(&result_buffer, results, result_in);         // Enqueue result-buffer invocations
    ihc_hls_component_run_all(result_buffer);                          // Run all enqueued invocations
    /////// Result buffer Testbench ///////
    
    
 // Check results with reference image
    bool pass = true;
    for(int y=0 ; y<row ; ++y){
        for(int x=0 ; x<col ; ++x){
            result[y][x] = results[y*col+x];
            if (result[y][x] != result_expected[y][x]){
                printf("ERROR: Expected %d, found %d\n", (int) result[y][x], (int) result_expected[y][x]);
                pass = false;
                printf("%d ", result[y][x]);
            }    
            
        }
    }
    /*
    for(int y=0 ; y<row ; ++y){
        for(int x=0 ; x<col ; ++x){
            result[y][x] = results[y*col+x];
            //if (result[y][x] != results[y*WIDTH+x]){
                //printf("ERROR: Expected %d, found %d\n", (int) result[y][x], (int) memory_tb[y*WIDTH+x]);
                //pass = false;
                printf("%d ", result[y][x]);
                
            
        }
    }
    */

    if (pass){
        printf("PASSED\n");
    }
    else{
        printf("FAILED\n");
    }

    return 0;
}
