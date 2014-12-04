/*****************************************************
 **  PIDX Parallel I/O Library                      **
 **  Copyright (c) 2010-2014 University of Utah     **
 **  Scientific Computing and Imaging Institute     **
 **  72 S Central Campus Drive, Room 3750           **
 **  Salt Lake City, UT 84112                       **
 **                                                 **
 **  PIDX is licensed under the Creative Commons    **
 **  Attribution-NonCommercial-NoDerivatives 4.0    **
 **  International License. See LICENSE.md.         **
 **                                                 **
 **  For information about this project see:        **
 **  http://www.cedmav.com/pidx                     **
 **  or contact: pascucci@sci.utah.edu              **
 **  For support: PIDX-support@visus.net            **
 **                                                 **
 *****************************************************/

#include "pidxtest.h"
#include <PIDX.h>

int test_multi_var_writer(struct Args args, int rank, int nprocs) 
{
#if PIDX_HAVE_MPI
  int i = 0, j = 0, k = 0;
  int ts, var, spv;
  int slice;
  int variable_count;
  int sub_div[3], local_offset[3];

  PIDX_file file;                                                // IDX file descriptor
  const char *output_file;                                                      // IDX File Name
  const int bits_per_block = 15;                                                // Total number of samples in each block = 2 ^ bits_per_block
  const int blocks_per_file = 256;                                               // Total number of blocks per file
  
  PIDX_variable* variable;                                       // variable descriptor
  double     **double_data;
  int* values_per_sample;
  
  //The command line arguments are shared by all processes
  MPI_Bcast(args.extents, 5, MPI_INT, 0, MPI_COMM_WORLD);
  MPI_Bcast(args.count_local, 5, MPI_INT, 0, MPI_COMM_WORLD);
  MPI_Bcast(&args.time_step, 1, MPI_INT, 0, MPI_COMM_WORLD);
  MPI_Bcast(&args.variable_count, 1, MPI_INT, 0, MPI_COMM_WORLD);
  MPI_Bcast(&args.output_file_template, 512, MPI_CHAR, 0, MPI_COMM_WORLD);
  
  variable_count = args.variable_count;
  variable = malloc(sizeof(*variable) * variable_count);
  memset(variable, 0, sizeof(*variable) * variable_count);
  
  values_per_sample = malloc(sizeof(*values_per_sample) * variable_count);
  memset(values_per_sample, 0, sizeof(*values_per_sample) * variable_count);
  
  //   Creating the filename 
  args.output_file_name = (char*) malloc(sizeof (char) * 512);
  sprintf(args.output_file_name, "%s%s", args.output_file_template, ".idx");

  //   Calculating every process's offset and count  
  sub_div[0] = (args.extents[0] / args.count_local[0]);
  sub_div[1] = (args.extents[1] / args.count_local[1]);
  sub_div[2] = (args.extents[2] / args.count_local[2]);
  local_offset[2] = (rank / (sub_div[0] * sub_div[1])) * args.count_local[2];
  slice = rank % (sub_div[0] * sub_div[1]);
  local_offset[1] = (slice / sub_div[0]) * args.count_local[1];
  local_offset[0] = (slice % sub_div[0]) * args.count_local[0];

  output_file = args.output_file_name;    
  
  PIDX_point global_bounding_box, local_offset_point, local_box_count_point;
  //PIDX_create_point(&global_bounding_box);
  //PIDX_create_point(&local_offset_point);
  //PIDX_create_point(&local_box_count_point);
  
  PIDX_set_point_5D((long long)args.extents[0], (long long)args.extents[1], (long long)args.extents[2], 1, 1, global_bounding_box);
  PIDX_set_point_5D((long long)local_offset[0], (long long)local_offset[1], (long long)local_offset[2], 0, 0, local_offset_point);
  PIDX_set_point_5D((long long)args.count_local[0], (long long)args.count_local[1], (long long)args.count_local[2], 1, 1, local_box_count_point);
  
  PIDX_enable_time_step_caching_ON();
  for (ts = 0; ts < args.time_step; ts++) 
  {
    double_data = malloc(sizeof(*double_data) * variable_count);
    memset(double_data, 0, sizeof(*double_data) * variable_count);
    
    PIDX_access access;
    PIDX_create_access(&access);

#if PIDX_HAVE_MPI
    PIDX_set_mpi_access(access, MPI_COMM_WORLD);
#else
    PIDX_set_default_access(access);
#endif
    
    PIDX_file_create(output_file, PIDX_file_trunc, access, &file);
    PIDX_set_dims(file, global_bounding_box);
    PIDX_set_current_time_step(file, ts);
    PIDX_set_block_size(file, bits_per_block);
    PIDX_set_block_count(file, blocks_per_file);
    PIDX_set_variable_count(file, variable_count);
    
    char variable_name[512];
    char data_type[512];
    
#if 0    
    /// IO with Flush (memory efficient)
    for(var = 0; var < variable_count; var++)
    {
      values_per_sample[var] = var + 1;
      double_data[var] = malloc(sizeof (double) * args.count_local[0] * args.count_local[1] * args.count_local[2]  * values_per_sample[var]);
      
      if(var % 2 == 0)
      {
	for (k = 0; k < args.count_local[2]; k++)
	  for (j = 0; j < args.count_local[1]; j++)
	    for (i = 0; i < args.count_local[0]; i++) 
	    {
	      long long index = (long long) (args.count_local[0] * args.count_local[1] * k) + (args.count_local[0] * j) + i;
	      for (spv = 0; spv < values_per_sample[var]; spv++)
		double_data[var][index * values_per_sample[var] + spv] = 100 + ((args.extents[0] * args.extents[1]*(local_offset[2] + k))+(args.extents[0]*(local_offset[1] + j)) + (local_offset[0] + i));
	    }
      }
      else
      {
	for (k = 0; k < args.count_local[2]; k++)
	  for (j = 0; j < args.count_local[1]; j++)
	    for (i = 0; i < args.count_local[0]; i++) 
	    {
	      long long index = (long long) (args.count_local[0] * args.count_local[1] * k) + (args.count_local[0] * j) + i;
	      for (spv = 0; spv < values_per_sample[var]; spv++)
		double_data[var][index * values_per_sample[var] + spv] = (rank + 1);
	    }
      }
      
      sprintf(variable_name, "variable_%d", var);
      sprintf(data_type, "%d*double64", values_per_sample[var]);
      PIDX_variable_create(file, variable_name, values_per_sample[var] * sizeof(double) * 8, data_type, &variable[var]);
      PIDX_append_and_write_variable(variable[var], local_offset_point, local_box_count_point, double_data[var], PIDX_row_major);
      PIDX_flush(file);
      
      free(double_data[var]);
      double_data[var] = 0;
    }
    
    PIDX_close(file);
    PIDX_close_access(access);
    
    free(double_data);
    double_data = 0;
#endif

#if 1    
    /// IO with no Flush (high performance)
    for(var = 0; var < variable_count; var++)
    {
      values_per_sample[var] = /*var + */1;
      double_data[var] = malloc(sizeof (double) * args.count_local[0] * args.count_local[1] * args.count_local[2]  * values_per_sample[var]);
      
      for (k = 0; k < args.count_local[2]; k++)
        for (j = 0; j < args.count_local[1]; j++)
          for (i = 0; i < args.count_local[0]; i++) 
          {
            long long index = (long long) (args.count_local[0] * args.count_local[1] * k) + (args.count_local[0] * j) + i;
            for (spv = 0; spv < values_per_sample[var]; spv++)
              double_data[var][index * values_per_sample[var] + spv] = 100 + ((args.extents[0] * args.extents[1]*(local_offset[2] + k))+(args.extents[0]*(local_offset[1] + j)) + (local_offset[0] + i));
          }
    }
    
    for(var = 0; var < variable_count; var++)
    {
      sprintf(variable_name, "variable_%d", var);
      sprintf(data_type, "%d*float64", values_per_sample[var]);
      PIDX_variable_create(file, variable_name, values_per_sample[var] * sizeof(double) * 8, data_type, &variable[var]);
      PIDX_append_and_write_variable(variable[var], local_offset_point, local_box_count_point, double_data[var], PIDX_row_major);
    }
    
    PIDX_close(file);
    PIDX_close_access(access);
    
    for(var = 0; var < variable_count; var++)
    {
      free(double_data[var]);
      double_data[var] = 0;
    }
#endif

    free(double_data);
    double_data = 0;
  }
  PIDX_enable_time_step_caching_OFF();
  
  free(variable);
  free(values_per_sample);
  free(args.output_file_name);
#endif
  return 0;
}

/*   prints usage instructions   */
void usage_multi_var_writer(void) 
{
  printf("Usage: test-multi-var-PIDX-writer -g 4x4x4 -l 2x2x2 -f Filename_ -t 4\n");
  printf("  -g: global dimensions\n");
  printf("  -l: local (per-process) dimensions\n");
  printf("  -f: IDX Filename\n");
  printf("  -t: number of timesteps\n");
  printf("\n");
  return;
}
