/*
 * CitcomCU is a Finite Element Code that solves for thermochemical
 * convection within a three dimensional domain appropriate for convection
 * within the Earth's mantle. Cartesian and regional-spherical geometries
 * are implemented. See the file README contained with this distribution
 * for further details.
 * 
 * Copyright (C) 1994-2005 California Institute of Technology
 * Copyright (C) 2000-2005 The University of Colorado
 *
 * Authors: Louis Moresi, Shijie Zhong, and Michael Gurnis
 *
 * For questions or comments regarding this software, you may contact
 *
 *     Luis Armendariz <luis@geodynamics.org>
 *     http://geodynamics.org
 *     Computational Infrastructure for Geodynamics (CIG)
 *     California Institute of Technology
 *     2750 East Washington Blvd, Suite 210
 *     Pasadena, CA 91007
 *
 * This program is free software; you can redistribute it and/or modify 
 * it under the terms of the GNU General Public License as published by 
 * the Free Software Foundation, either version 2 of the License, or any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

//
// A particle method is implemented by Shijie Zhong in July 2002. Later,
// the ratio method to convert particle into C field was added in 2004. 
//


/* ================================================ */

#include <malloc.h>
#include <sys/types.h>
#include <math.h>
#include <mpi.h>
#include "element_definitions.h"
#include "global_defs.h"

/* *INDENT-OFF* */
static float xxsh[5][3] = {  {0.0,  0.0,  0.0},
                             {0.0, -1.0, -1.0},
                             {0.0,  1.0, -1.0},
                             {0.0,  1.0,  1.0},
                             {0.0, -1.0,  1.0}  };
/* *INDENT-ON* */


void Runge_Kutta(struct All_variables *E, float *C, float *V[4], int on_off)
{
	int i;
	double temp1, temp2, temp3;

	/*   predicted velocity Vpred at predicted marker positions at t+dt  */
	velocity_markers(E, V, on_off);

	/*   final marker positions at t+dt from modified Euler */
	if(E->control.CART3D)
	{
		for(i = 1; i <= E->advection.markers; i++)
		{
			E->XMC[1][i] = E->XMC[1][i] + 0.5 * E->advection.timestep * (E->VO[1][i] + E->Vpred[1][i]);
			E->XMC[2][i] = E->XMC[2][i] + 0.5 * E->advection.timestep * (E->VO[2][i] + E->Vpred[2][i]);
			E->XMC[3][i] = E->XMC[3][i] + 0.5 * E->advection.timestep * (E->VO[3][i] + E->Vpred[3][i]);
		}
	}
	else if(E->control.Rsphere)
	{
		for(i = 1; i <= E->advection.markers; i++)
		{

			temp1 = E->XMC[1][i] + 0.5 * E->advection.timestep * (E->VO[1][i] + E->Vpred[1][i]) / E->XMC[3][i];
			temp2 = E->XMC[2][i] + 0.5 * E->advection.timestep * (E->VO[2][i] + E->Vpred[2][i]) / (E->XMC[3][i] * sin(E->XMC[1][i]));
			temp3 = E->XMC[3][i] + 0.5 * E->advection.timestep * (E->VO[3][i] + E->Vpred[3][i]);

			E->XMC[1][i] = temp1;
			E->XMC[2][i] = temp2;
			E->XMC[3][i] = temp3;
		}
	}


	/* assign to elements */
	transfer_marker_properties(E,C,on_off);


	E->advection.markers_g = -1;
	if(E->advection.timesteps % 10 == 0)
		MPI_Allreduce(&E->advection.markers, &E->advection.markers_g, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

	return;
}

/* ================================================ */

void Euler(struct All_variables *E, float *C, float *V[4], int on_off)
{
	int i;
	double temp1, temp2, temp3;
	/*   velocity VO at t and x=XMC  */
	velocity_markers(E, V, on_off);
	/*   predicted marker positions at t+dt  */
	if(E->control.CART3D)
	{
		for(i = 1; i <= E->advection.markers; i++)
		{
			E->XMCpred[1][i] = E->XMC[1][i] + E->advection.timestep * E->VO[1][i];
			E->XMCpred[2][i] = E->XMC[2][i] + E->advection.timestep * E->VO[2][i];
			E->XMCpred[3][i] = E->XMC[3][i] + E->advection.timestep * E->VO[3][i];
		}
	}
	else if(E->control.Rsphere)
	{
		for(i = 1; i <= E->advection.markers; i++)
		{
			temp1 = E->XMC[1][i] + E->advection.timestep * E->VO[1][i] / E->XMC[3][i];
			temp2 = E->XMC[2][i] + E->advection.timestep * E->VO[2][i] / (E->XMC[3][i] * sin(E->XMC[1][i]));
			temp3 = E->XMC[3][i] + E->advection.timestep * E->VO[3][i];
			E->XMCpred[1][i] = temp1;
			E->XMCpred[2][i] = temp2;
			E->XMCpred[3][i] = temp3;
		}
	}


	/*   predicted compositional field at t+dt  */

	/* assign to processor and then to elements */
	transfer_marker_properties(E,C,on_off);

	return;
}

/* ================================================ 
 ================================================  */
void transfer_markers_processors(struct All_variables *E, int on_off)
{
  //FILE *fp;
  //char output_file[255];
  int i, proc, neighbor, no_transferred, no_received,asize,asizet;
  
  static int been = 0;
  static int markers;
  
  if(been == 0){

    markers = E->advection.markers / 10;
    asize = (markers + 1) * (E->mesh.nsd * 2 );
    asizet = (markers + 1) * (E->mesh.nsd * 2 + E->tracers_track_strain);
    
    if( E->parallel.no_neighbors >= MAX_NEIGHBORS)
      myerror("error, number of neighbors out of bounds",E);
    for(neighbor = 1; neighbor <= E->parallel.no_neighbors; neighbor++){
      E->parallel.traces_transfer_index[neighbor] = (int *)safe_malloc((markers + 1) * sizeof(int));
      E->RVV[neighbor] = (float *)safe_malloc(asizet * sizeof(int));
      E->RXX[neighbor] = (CITCOM_XMC_PREC *)safe_malloc(asize * sizeof(CITCOM_XMC_PREC));
      E->RINS[neighbor] = (int *)safe_malloc((markers + 1) * (2 + E->tracers_add_flavors) * sizeof(int));
      E->PVV[neighbor] = (float *)safe_malloc(asizet  * sizeof(int));
      E->PXX[neighbor] = (CITCOM_XMC_PREC *)safe_malloc(asize * sizeof(CITCOM_XMC_PREC));
      E->PINS[neighbor] = (int *)safe_malloc((markers + 1) * (2 + E->tracers_add_flavors) * sizeof(int));
      
    }
    E->traces_leave_index = (int *)safe_malloc((markers + 1) * sizeof(int));
    been++;
  }
  
  /* check which processor tracer will be moved to */
  move_tracers_to_neighbors(E,on_off);

  // prepare for transfer 
  no_transferred = 0;
  for(neighbor = 1; neighbor <= E->parallel.no_neighbors; neighbor++){
    no_transferred += E->parallel.traces_transfer_number[neighbor];
  }
  
  /* this is the bad routine!!! */
  prepare_transfer_arrays(E);

  exchange_number_rec_markers(E);

  no_received = 0;
  for(neighbor = 1; neighbor <= E->parallel.no_neighbors; neighbor++){
    no_received += E->parallel.traces_receive_number[neighbor];
  }
  
  E->advection.markers1 = E->advection.markers + no_received - no_transferred;
  
  if(E->advection.markers1 > E->advection.markers_uplimit){
    fprintf(E->fp, "number of markers over the limit %d\n", E->advection.markers1);
    fflush(E->fp);
    parallel_process_termination();
  }
  
  exchange_markers(E);
  
  unify_markers_array(E, no_transferred, no_received);
  
  
  return;
}

/* 
   extracted move around the tracers routines 
   for both periodic and reflective
*/
void move_tracers_to_neighbors(struct All_variables *E, int on_off)
{

  int neighbor,i,proc;
  const int me = E->parallel.me;
  CITCOM_XMC_PREC xloc,yloc,zloc;
  static int been_here=0;
  static CITCOM_XMC_PREC loc_xg1[3],loc_xg2[3],loc_xrange[3];
  if(!been_here){
    /* 
       set up geometric bounds slightly differently for periodic BCs
       the XG1/2 arrays have a CITCOM_TRACER_EPS_MARGIN margin by
       default, seems like a bad idea for periodic BCs
       
     */
    if(E->mesh.periodic_x){	/* use exact values */
      if(E->control.CART3D){	
	loc_xg1[1] = 0.0;
	loc_xg2[1] = E->mesh.layer[1];
      }else{
	loc_xg1[1] = E->sphere.ti * M_PI/180.0; 
	loc_xg2[1] = E->sphere.to * M_PI/180.0;
      }
    }else{			/* use the values that have a margin
				   subtracted */
      loc_xg1[1] = E->XG1[1];
      loc_xg2[1] = E->XG2[1];
    }
    if(E->mesh.periodic_y){	/* use exact values */
      if(E->control.CART3D){	
	loc_xg1[2] = 0.0;
	loc_xg2[2] = E->mesh.layer[2];
      }else{
	loc_xg1[2] = E->sphere.fi * M_PI/180.0; 
	loc_xg2[2] = E->sphere.fo * M_PI/180.0;
      }
    }else{			/* use the values that have a margin
				   subtracted */
      loc_xg1[2] = E->XG1[2];
      loc_xg2[2] = E->XG2[2];
    }
    /* range to shuffle, if needed */
    loc_xrange[1] = loc_xg2[1]-loc_xg1[1];
    loc_xrange[2] = loc_xg2[2]-loc_xg1[2];
    been_here = 1;
    /* end of init branch */
  }
  /* 
     reset the number of transfers 
  */
  for(neighbor = 0; neighbor <= E->parallel.no_neighbors; neighbor++)
    E->parallel.traces_transfer_number[neighbor] = 0;

  if(E->mesh.periodic_x || E->mesh.periodic_y){
    /* 
       this branch allows for periodic boundary conditions 
    */
    if(on_off == 1){// use XMC
      for(i = 1; i <= E->advection.markers; i++){
	E->traces_leave[i] = 0;
	
	if(E->Element[E->CElement[i]] & SIDEE){
	  if(E->mesh.periodic_x){ /* let's see if this works */
	    if(E->XMC[1][i] > loc_xg2[1])
	      E->XMC[1][i] -= loc_xrange[1];
	    if(E->XMC[1][i] < loc_xg1[1])
	      E->XMC[1][i] += loc_xrange[1];
	  }else{
	    E->XMC[1][i] = min(E->XMC[1][i], E->XG2[1]);
	    E->XMC[1][i] = max(E->XMC[1][i], E->XG1[1]);
	  }
	  if(E->mesh.periodic_y){
	    if(E->XMC[2][i] > loc_xg2[2])
	      E->XMC[2][i] -= loc_xrange[2];
	    if(E->XMC[2][i] < loc_xg1[2])
	      E->XMC[2][i] += loc_xrange[2];
	  }else{
	    E->XMC[2][i] = min(E->XMC[2][i], E->XG2[2]);
	    E->XMC[2][i] = max(E->XMC[2][i], E->XG1[2]);
	  }
	  E->XMC[3][i] = min(E->XMC[3][i], E->XG2[3]);
	  E->XMC[3][i] = max(E->XMC[3][i], E->XG1[3]);
	  
	  proc = locate_processor(E, E->XMC[1][i], E->XMC[2][i], E->XMC[3][i]);
	  
	  if(proc != me){
	    E->traces_leave[i] = 1;
	    neighbor = E->parallel.neighbors_rev[proc];
	    E->parallel.traces_transfer_index[neighbor][E->parallel.traces_transfer_number[neighbor]] = i;
	    E->parallel.traces_transfer_number[neighbor]++;
	  }
	}
      }	/* end marker loop */
      /* end on_off == 1 */
    }else if(on_off == 0){
      //
      // same as above, but use XMCpred
      //
      for(i = 1; i <= E->advection.markers; i++){
	E->traces_leave[i] = 0;
	if(E->Element[E->CElement[i]] & SIDEE){

	  if(E->mesh.periodic_x){ // same as above
	    if(E->XMCpred[1][i] > loc_xg2[1])
	      E->XMCpred[1][i] -= loc_xrange[1];
	    if(E->XMCpred[1][i] < loc_xg1[1])
	      E->XMCpred[1][i] += loc_xrange[1];
	  }else{
	    E->XMCpred[1][i] = min(E->XMCpred[1][i], E->XG2[1]);
	    E->XMCpred[1][i] = max(E->XMCpred[1][i], E->XG1[1]);
	  }
	  if(E->mesh.periodic_y){
	    if(E->XMCpred[2][i] > loc_xg2[2])
	      E->XMCpred[2][i] -= loc_xrange[2];
	    if(E->XMCpred[2][i] < loc_xg1[2])
	      E->XMCpred[2][i] += loc_xrange[2];
	  }else{
	    E->XMCpred[2][i] = min(E->XMCpred[2][i], E->XG2[2]);
	    E->XMCpred[2][i] = max(E->XMCpred[2][i], E->XG1[2]);
	  }
	  E->XMCpred[3][i] = min(E->XMCpred[3][i], E->XG2[3]);
	  E->XMCpred[3][i] = max(E->XMCpred[3][i], E->XG1[3]);
	  
	  proc = locate_processor(E, E->XMCpred[1][i], E->XMCpred[2][i], E->XMCpred[3][i]);
	  
	  if(proc != me){
	    E->traces_leave[i] = 1;
	    neighbor = E->parallel.neighbors_rev[proc];
	    E->parallel.traces_transfer_index[neighbor][E->parallel.traces_transfer_number[neighbor]] = i;
	    E->parallel.traces_transfer_number[neighbor]++;
	  }
	}
      }	/* end marker loop */
    } /* end on_off = 0 */
    
    /* end periodic branch */
  }else{
    /* 
       this is the regular branch wihtout periodic BCs 
    */
    if(on_off == 1){// use XMC
      for(i = 1; i <= E->advection.markers; i++){
	E->traces_leave[i] = 0;
	if(E->Element[E->CElement[i]] & SIDEE){
	  
	  E->XMC[1][i] = min(E->XMC[1][i], E->XG2[1]);
	  E->XMC[1][i] = max(E->XMC[1][i], E->XG1[1]);
	  E->XMC[2][i] = min(E->XMC[2][i], E->XG2[2]);
	  E->XMC[2][i] = max(E->XMC[2][i], E->XG1[2]);
	  E->XMC[3][i] = min(E->XMC[3][i], E->XG2[3]);
	  E->XMC[3][i] = max(E->XMC[3][i], E->XG1[3]);
	  
	  proc = locate_processor(E, E->XMC[1][i], E->XMC[2][i], E->XMC[3][i]);
	  
	  if(proc != me){
	    E->traces_leave[i] = 1;
	    neighbor = E->parallel.neighbors_rev[proc];
	    E->parallel.traces_transfer_index[neighbor][E->parallel.traces_transfer_number[neighbor]] = i;
	    E->parallel.traces_transfer_number[neighbor]++;
	  }
	}
      }
    }else if(on_off == 0){// same as above, but use XMCpred
      
      for(i = 1; i <= E->advection.markers; i++){
	E->traces_leave[i] = 0;
	
	if(E->Element[E->CElement[i]] & SIDEE){
	  E->XMCpred[1][i] = min(E->XMCpred[1][i], E->XG2[1]);
	  E->XMCpred[1][i] = max(E->XMCpred[1][i], E->XG1[1]);
	  E->XMCpred[2][i] = min(E->XMCpred[2][i], E->XG2[2]);
	  E->XMCpred[2][i] = max(E->XMCpred[2][i], E->XG1[2]);
	  E->XMCpred[3][i] = min(E->XMCpred[3][i], E->XG2[3]);
	  E->XMCpred[3][i] = max(E->XMCpred[3][i], E->XG1[3]);
	  
	  proc = locate_processor(E, E->XMCpred[1][i], E->XMCpred[2][i], E->XMCpred[3][i]);
	  
	  if(proc != me){
	    E->traces_leave[i] = 1;
	    neighbor = E->parallel.neighbors_rev[proc];
	    E->parallel.traces_transfer_index[neighbor][E->parallel.traces_transfer_number[neighbor]] = i;
	    E->parallel.traces_transfer_number[neighbor]++;
	  }
	  
	}
      }
    }
    /* end regular branch */
  }
}
  

void unify_markers_array(struct All_variables *E, int no_tran, int no_recv)
{
  int i, j,k,l;
	int ii, jj, kk;
	int neighbor, no_trans1;
	static int been_here = 0,tscol,nsd2,nsd2t,rioff;
	if(!been_here){
	  tscol = (E->tracers_track_fse)?(10):(1);
	  nsd2 = E->mesh.nsd * 2;	/*  */
	  nsd2t = nsd2 + tscol; /* for those arrays that have strain in them */
	  rioff = 2 + E->tracers_add_flavors;
	  been_here = 1;
	}


	ii = 0;
	for(i = 1; i <= E->advection.markers; i++)
		if(E->traces_leave[i])
		{
			ii++;
			E->traces_leave_index[ii] = i;
		}

	no_trans1 = ii;

	if(no_trans1 != no_tran)
	{
		fprintf(E->fp, "problems with no_trans1 no_tran %d %d\n", no_trans1, no_tran);
		fflush(E->fp);
		parallel_process_termination();
	}

	ii = jj = 0;
	if(E->advection.markers1 >= E->advection.markers)
	{
		for(neighbor = 1; neighbor <= E->parallel.no_neighbors; neighbor++)
		{
			for(j = 0; j < E->parallel.traces_receive_number[neighbor]; j++)
			{
				jj++;
				if(jj <= no_trans1)
					ii = E->traces_leave_index[jj];
				else
					ii = E->advection.markers + jj - no_trans1;

				E->VO[1][ii] = E->RVV[neighbor][j * nsd2t];
				E->VO[2][ii] = E->RVV[neighbor][j * nsd2t + 1];
				E->VO[3][ii] = E->RVV[neighbor][j * nsd2t + 2];
				E->Vpred[1][ii] = E->RVV[neighbor][j * nsd2t + 3];
				E->Vpred[2][ii] = E->RVV[neighbor][j * nsd2t + 4];
				E->Vpred[3][ii] = E->RVV[neighbor][j * nsd2t + 5];
				if(E->tracers_track_strain)
				  E->tracer_strain[ii*tscol] = E->RVV[neighbor][j * nsd2t + 6];
				if(E->tracers_track_fse)
				  for(l=0;l<9;l++)
				    E->tracer_strain[ii*tscol+1+l] = E->RVV[neighbor][j * nsd2t + 7 + l];
				
				E->XMC[1][ii] = E->RXX[neighbor][j * nsd2];
				E->XMC[2][ii] = E->RXX[neighbor][j * nsd2 + 1];
				E->XMC[3][ii] = E->RXX[neighbor][j * nsd2 + 2];
				E->XMCpred[1][ii] = E->RXX[neighbor][j * nsd2 + 3];
				E->XMCpred[2][ii] = E->RXX[neighbor][j * nsd2 + 4];
				E->XMCpred[3][ii] = E->RXX[neighbor][j * nsd2 + 5];
				E->C12[ii] =      E->RINS[neighbor][j * rioff];
				E->CElement[ii] = E->RINS[neighbor][j * rioff + 1];
				for(k=0;k < E->tracers_add_flavors;k++)
				  E->tflavors[ii][k] = E->RINS[neighbor][j * rioff + 2 + k];
				E->traces_leave[ii] = 0;
			}
		}
	}

	else if(E->advection.markers1 < E->advection.markers)
	{
		for(neighbor = 1; neighbor <= E->parallel.no_neighbors; neighbor++)
		{
			for(j = 0; j < E->parallel.traces_receive_number[neighbor]; j++)
			{
				jj++;
				ii = E->traces_leave_index[jj];
				E->VO[1][ii] = E->RVV[neighbor][j * nsd2t];
				E->VO[2][ii] = E->RVV[neighbor][j * nsd2t + 1];
				E->VO[3][ii] = E->RVV[neighbor][j * nsd2t + 2];
				E->Vpred[1][ii] = E->RVV[neighbor][j * nsd2t + 3];
				E->Vpred[2][ii] = E->RVV[neighbor][j * nsd2t + 4];
				E->Vpred[3][ii] = E->RVV[neighbor][j * nsd2t + 5];
				if(E->tracers_track_strain)
				  E->tracer_strain[ii*tscol] = E->RVV[neighbor][j * nsd2t + 6];
				if(E->tracers_track_fse)
				  for(l=0;l<9;l++)
				    E->tracer_strain[ii*tscol+1+l] = E->RVV[neighbor][j * nsd2t + 7 + l];

				E->XMC[1][ii] = E->RXX[neighbor][j * nsd2];
				E->XMC[2][ii] = E->RXX[neighbor][j * nsd2 + 1];
				E->XMC[3][ii] = E->RXX[neighbor][j * nsd2 + 2];
				E->XMCpred[1][ii] = E->RXX[neighbor][j * nsd2 + 3];
				E->XMCpred[2][ii] = E->RXX[neighbor][j * nsd2 + 4];
				E->XMCpred[3][ii] = E->RXX[neighbor][j * nsd2 + 5];
				E->C12[ii] =      E->RINS[neighbor][j * rioff];
				E->CElement[ii] = E->RINS[neighbor][j * rioff + 1];
				for(k=0;k < E->tracers_add_flavors;k++)
				  E->tflavors[ii][k] = E->RINS[neighbor][j * rioff + 2 + k];
				E->traces_leave[ii] = 0;
			}
		}
// take elements from the back of traces queue and fill the front empty spots

		i = E->advection.markers;
		kk = jj;

		do
		{
			if(E->traces_leave[i] == 0)
			{
				jj++;
				kk++;
				if(jj <= no_trans1)
				{
					ii = E->traces_leave_index[jj];
					E->VO[1][ii] = E->VO[1][i];
					E->VO[2][ii] = E->VO[2][i];
					E->VO[3][ii] = E->VO[3][i];
					E->Vpred[1][ii] = E->Vpred[1][i];
					E->Vpred[2][ii] = E->Vpred[2][i];
					E->Vpred[3][ii] = E->Vpred[3][i];
					E->XMC[1][ii] = E->XMC[1][i];
					E->XMC[2][ii] = E->XMC[2][i];
					E->XMC[3][ii] = E->XMC[3][i];
					E->XMCpred[1][ii] = E->XMCpred[1][i];
					E->XMCpred[2][ii] = E->XMCpred[2][i];
					E->XMCpred[3][ii] = E->XMCpred[3][i];
					E->C12[ii]      = E->C12[i];
					E->CElement[ii] = E->CElement[i];
					for(k=0;k < E->tracers_add_flavors;k++)
					  E->tflavors[ii][k] = E->tflavors[i][k];
					if(E->tracers_track_strain)
					  E->tracer_strain[ii] = E->tracer_strain[i];
					if(E->tracers_track_fse)
					  for(k=0;k<9;k++)
					    E->tracer_strain[ii*tscol+1+k] =  E->tracer_strain[i*tscol+1+k];

					
					E->traces_leave[ii] = 0;
				}
			}
			else if(E->traces_leave[i])
			{
				kk++;
			}
			i--;
		} while(kk < no_trans1);

	}

	E->advection.markers = E->advection.markers1;

	return;
}


void prepare_transfer_arrays(struct All_variables *E)
{
  int j, part, neighbor, k1, k2, k3,k,l;
  static int asize, asizet,markers,bsize,tscol,been_here = 0;
  if(!been_here){
    markers = E->advection.markers / 10;
    asize = (markers + 1) * (E->mesh.nsd * 2 );
    /*  */
    /* strain entries per tracer */
    tscol = (E->tracers_track_fse)?(10):(1);
    asizet = (markers + 1) * (E->mesh.nsd * 2 + tscol);
    bsize = (markers + 1) * (2 + E->tracers_add_flavors);
   
    been_here = 1;
  }
  parallel_process_sync();

  //if(E->parallel.me==0)fprintf(stderr,"ta 1 ok\n");
	for(neighbor = 1; neighbor <= E->parallel.no_neighbors; neighbor++)
	{
		k1 = k2 = k3 = 0;
		for(j = 0; j < E->parallel.traces_transfer_number[neighbor]; j++)
		{
			part = E->parallel.traces_transfer_index[neighbor][j];
			//if((part > E->advection.markers)||(part<1)){fprintf(stderr,"pta: out of bounds %i %i\n",part,E->advection.markers);myerror("pta out of bounds",E);}
			//if(E->monitor.solution_cycles>199)fprintf(stderr,"%i --- %i/%i %i/%i %i\n",E->parallel.me,neighbor,E->parallel.no_neighbors,j,E->parallel.traces_transfer_number[neighbor],part);
			
			//if(k1+6 >= asize){fprintf(stderr,"k1 %i asize %i out of bounds\n",k1,asize);myerror("exit",E);};
			E->PVV[neighbor][k1++] = E->VO[1][part];
			E->PVV[neighbor][k1++] = E->VO[2][part];
			E->PVV[neighbor][k1++] = E->VO[3][part];
			E->PVV[neighbor][k1++] = E->Vpred[1][part];
			E->PVV[neighbor][k1++] = E->Vpred[2][part];
			E->PVV[neighbor][k1++] = E->Vpred[3][part];
			if(E->tracers_track_strain)
			  E->PVV[neighbor][k1++] = E->tracer_strain[part*tscol];
			if(E->tracers_track_fse)
			  for(l=0;l<9;l++)
			    E->PVV[neighbor][k1++] = E->tracer_strain[part*tscol+1+l];
			     
			//if(k2+6 >= asize){fprintf(stderr,"k2 %i asize %i out of bounds\n",k2,asize);myerror("exit",E);};
			E->PXX[neighbor][k2++] = E->XMC[1][part];
			E->PXX[neighbor][k2++] = E->XMC[2][part];
			E->PXX[neighbor][k2++] = E->XMC[3][part];
			E->PXX[neighbor][k2++] = E->XMCpred[1][part];
			E->PXX[neighbor][k2++] = E->XMCpred[2][part];
			E->PXX[neighbor][k2++] = E->XMCpred[3][part];


			//if(k3+2+E->tracers_add_flavors >= bsize){fprintf(stderr,"k3 %i bsize %i out of bounds\n",k3,bsize);myerror("exit",E);};
			E->PINS[neighbor][k3++] = E->C12[part];
			E->PINS[neighbor][k3++] = E->CElement[part];
			for(k=0;k < E->tracers_add_flavors;k++)
			  E->PINS[neighbor][k3++] = E->tflavors[part][k];

		}
	}
	//if(E->parallel.me==0)fprintf(stderr,"ta 2 ok\n");
	return;
}

// like get_element, assuming uniform mesh in x and y
int locate_processor(struct All_variables *E, 
		     CITCOM_XMC_PREC XMC1, 
		     CITCOM_XMC_PREC XMC2, 
		     CITCOM_XMC_PREC XMC3)
{
  int proc, m1, m2, m3;
  const int npx = E->parallel.nprocx - 1;
  const int npy = E->parallel.nprocy - 1;
  const int npz = E->parallel.nprocz - 1;
  
  if(XMC1 > E->XP[1][E->lmesh.nox])
    m1 = min(npx, E->parallel.me_loc[1] + 1);
  else if(XMC1 < E->XP[1][1])
    m1 = max(0, E->parallel.me_loc[1] - 1);
  else
    m1 = E->parallel.me_loc[1];
  
  if(XMC2 > E->XP[2][E->lmesh.noy])
    m2 = min(npy, E->parallel.me_loc[2] + 1);
  else if(XMC2 < E->XP[2][1])
    m2 = max(0, E->parallel.me_loc[2] - 1);
  else
    m2 = E->parallel.me_loc[2];
  
  if(XMC3 > E->XP[3][E->lmesh.noz])
    m3 = min(npz, E->parallel.me_loc[3] + 1);
  else if(XMC3 < E->XP[3][1])
    m3 = max(0, E->parallel.me_loc[3] - 1);
  else
    m3 = E->parallel.me_loc[3];
  
  proc = m3 + m1 * E->parallel.nprocz + m2 * E->parallel.nprocxz;
#ifdef DEBUG
  if((proc < 0) || (proc >= E->parallel.nproc)){
    fprintf(stderr,"cpu %i m %i %i %i nproc %i loc %g %g %g\n",E->parallel.me,m1,m2,m3,proc,XMC1,XMC2,XMC3);
    myerror("out of bounds in locate processor",E);
  }
#endif
  return (proc);
}


/* 

   assign all properties from markers to elements , and then to nodes

   con: 1: use old location 0: use new location

*/

void transfer_marker_properties(struct All_variables *E, float *C, int con)
{

  /* assign to processor */
  transfer_markers_processors(E, con);

  /* assign element numbers to markers */
  element_markers(E, con);
  /* 
     get elemental values from markers 
  */
  get_C_from_markers(E, C); /* composition */
  
  if(E->tracers_add_flavors)
    get_CF_from_markers(E,E->CF); /* flavors */
  //if(E->parallel.me == 0)fprintf(stderr,"transfer_marker_properties: t %g dt %g con %i\n",E->monitor.elapsed_time,E->advection.timestep,con);
				 
  if(E->tracers_track_strain){	/* update tracer strain */
    if(!con)
      evolve_tracer_strain(E);
    get_strain_from_markers(E,E->strain);
  }
}


/* ================================================ 
 ================================================  */
void get_C_from_markers(struct All_variables *E, float *C)
{
	int el, i, imark, j, node;
	float temp3, temp1, temp0;
	static int been_here = 0;
	static int *element[3];

	//const int elx = E->lmesh.elx;
	//const int elz = E->lmesh.elz;
	//const int ely = E->lmesh.ely;
	//const int nox = E->lmesh.nox;
	//const int noz = E->lmesh.noz;
	const int nno = E->lmesh.nno;
	const int nel = E->lmesh.nel;
	const int dims = E->mesh.nsd;
	const int ends = enodes[dims];
	const int lev = E->mesh.levmax;

	if(been_here == 0)
	{
		been_here++;
		element[0] = (int *)safe_malloc((nel + 1) * sizeof(int));
		element[1] = (int *)safe_malloc((nel + 1) * sizeof(int));
	}

	for(el = 1; el <= nel; el++)
	{
		element[0][el] = 0;
		element[1][el] = 0;
	}


	for(i = 1; i <= nno; i++)
	{
		C[i] = 0.0;
	}
	
	/* this will assign C=1 on top left side */
	if(E->mesh.slab_influx_side_bc)
	  composition_apply_slab_influx_side_bc(E);
	

	/* for each element, count dense and regular marks  */
	for(imark = 1; imark <= E->advection.markers; imark++)
	{
		element[E->C12[imark]][E->CElement[imark]]++;
	}
	if(E->tracers_assign_dense_only){
	  /* 
	     generally, a bad idea, counting dense only
	  */
	  for(el = 1; el <= nel; el++)
	    {
	      temp1 = element[1][el];
	      if(element[1][el]){
		temp3 = temp1 / (float)E->advection.markers_per_ele;	/* elemental C */
		if(temp3 > 1)
		  temp3 = 1.0; /* against tracer clumping */
	      }else
		temp3 = 0.0;	
	      for(j = 1; j <= ends; j++)
		{
		  node = E->ien[el].node[j];
		  C[node] += E->TWW[lev][el].node[j] * temp3;
		}
	      E->CE[el] = temp3;
	    }
	}else{

	  /* 
	     ratio method 
	  */
	  for(el = 1; el <= nel; el++)
	    {
	      temp0 = element[0][el];
	      temp1 = element[1][el];
	      if(element[0][el] || element[1][el])
		temp3 = temp1 / (temp0 + temp1);	/* elemental C */
	      else
		temp3 = E->CE[el];	/* elemental C */
	      for(j = 1; j <= ends; j++)
		{
		  node = E->ien[el].node[j];
		  C[node] += E->TWW[lev][el].node[j] * temp3;
		}
	      E->CE[el] = temp3;
	    }
	}
	/* go from elements to nodes */
	exchange_node_f20(E, C, E->mesh.levmax);

	for(node = 1; node <= nno; node++)
	{
 		C[node] = C[node] * E->Mass[node];
	}

	return;
}


/* 

obtain nodal flavor from markers

*/
void get_CF_from_markers(struct All_variables *E, int **CF)
{
  int el, itracer, imark, jnode, node,k,i,j;
  float *dmin,dist;
  static int method = 0,been_here = 0;
  float temp3, temp1, temp0;
  static int *element[3];

  const int nno = E->lmesh.nno;
  const int nel = E->lmesh.nel;
  const int dims = E->mesh.nsd;
  const int ends = enodes[dims];
  const int lev = E->mesh.levmax;
  const int nno1 = nno + 1;

  if(!been_here){
    /* decide on method */
    for(i=-1,k=0;k < E->tracers_add_flavors;k++)
      if(E->tmaxflavor[k] > i)
	i = E->tmaxflavor[k];
    if(i > 1)			/* any of the flavors is more than one */
      method = 0;
    else{
      method = 1;		/* for counting entries */
      element[0] = (int *)malloc((nel + 1) * sizeof(int));
      element[1] = (int *)malloc((nel + 1) * sizeof(int));
    }
    if(E->parallel.me==0)
      if(method == 0)
	fprintf(stderr,"get_CF_from_markers: using closest tracer method\n");
      else
	fprintf(stderr,"get_CF_from_markers: using mean flavor method for range 0..1\n");
    been_here = 1;
  }
  dmin = (float *)malloc((nno1) * sizeof(float));
  switch(method){
   case 0:			
    /* determine nodal flavor based on the closest tracer */
    for(k=0;k < nno1;k++)
      dmin[k] = 1e10;
    for(itracer=1;itracer <= E->advection.markers;itracer++){
      el = E->CElement[itracer] ;
      for(jnode = 1; jnode <= ends; jnode++){	/* loop through the nodes within this element */
	node = E->ien[el].node[jnode];
	dist = (float) distance_to_node(E->XMC[1][itracer],E->XMC[2][itracer],E->XMC[3][itracer],E,node);
	if(dmin[node] > dist){
	  dmin[node] = dist;
	  for(k=0;k < E->tracers_add_flavors;k++)
	    CF[k][node] = E->tflavors[itracer][k];
	}
      }
    }
    break;
  case 1:		
    /* 
       mean flavor method : compute based on the mean flavor of each element
    */
    for(k=0;k < E->tracers_add_flavors;k++){
      for(el = 1; el <= nel; el++){
	element[0][el] = element[1][el] = 0;
      }
      for(i = 1; i <= nno; i++){
	dmin[i] = 0.0;
      }
      for(imark = 1; imark <= E->advection.markers; imark++){
	element[E->tflavors[imark][k]][E->CElement[imark]]++;
      }
      for(el = 1; el <= nel; el++){
	temp0 = element[0][el];
	temp1 = element[1][el];
	if(element[0][el] || element[1][el])
	  temp3 = temp1 / (temp0 + temp1);	/* mean elemental CF */
	else
	  temp3 = 0;		/* no tracers */
	for(j = 1; j <= ends; j++){
	  node = E->ien[el].node[j];
	  dmin[node] += E->TWW[lev][el].node[j] * temp3;
	}
      }
      exchange_node_f20(E, dmin, E->mesh.levmax);
      for(node = 1; node <= nno; node++){
	CF[k][node] = (int) (dmin[node] * E->Mass[node] +.5);
      }
    }
    break;			/*  */
  default:
    myerror("method for flavor interpolation undefined",E);
    break;
  }
  free(dmin);
}
/* 

   obtain strain from markers and assign to all nodes

*/
void get_strain_from_markers(struct All_variables *E, float *strain)
{
  int el, itracer, imark, jnode, node,i,j;
   float *element_strain;
   int *element_count;
   static int been_here = 0,tscol;
   const int nno = E->lmesh.nno;
   const int nel = E->lmesh.nel;
   const int dims = E->mesh.nsd;
   const int ends = enodes[dims];
   const int lev = E->mesh.levmax;
   if(!been_here){
     tscol = (E->tracers_track_fse)?(10):(1);
     been_here = 1;
   }
 
   element_strain = (float *)calloc((nel + 1), sizeof(float));
   element_count = (int *)calloc((nel + 1),sizeof(int));
   /* 
        compute based on the mean strain of each element
   */
   for(i = 1; i <= nno; i++){
     strain[i] = 0.0;
   }
   for(imark = 1; imark <= E->advection.markers; imark++){
     el = E->CElement[imark];
     element_strain[el] += E->tracer_strain[imark*tscol];
     element_count[el] += 1;
   }
   for(el = 1; el <= nel; el++){
     if(element_count[el])
       element_strain[el] /= (float)element_count[el];
     /* got average element strain */
     for(j = 1; j <= ends; j++){
       node = E->ien[el].node[j];
       strain[node] += E->TWW[lev][el].node[j] * element_strain[el];
     }
   }
   exchange_node_f20(E, strain, E->mesh.levmax);
   for(node = 1; node <= nno; node++){
     strain[node] *= E->Mass[node];
   }
   free(element_strain);
   free(element_count);
}




/* ================================================ */
void element_markers(struct All_variables *E, int con)
{
	int i, el;
	double dX[4];

	E->advection.markerIX = 1;
	E->advection.markerIY = 1;
	E->advection.markerIZ = 1;

	if(con)
	{
		for(i = 1; i <= E->advection.markers; i++)
		{
			el = get_element(E, E->XMC[1][i], E->XMC[2][i], E->XMC[3][i], dX);
			E->CElement[i] = el;
		}
	}
	else
	{
		for(i = 1; i <= E->advection.markers; i++)
		{
			el = get_element(E, E->XMCpred[1][i], E->XMCpred[2][i], E->XMCpred[3][i], dX);
			E->CElement[i] = el;
		}
	}

	return;
}

/* ================================================ */
void velocity_markers(struct All_variables *E, float *V[4], int con)
{
	//FILE *fp0;
	//char filename1[100];
	//int eln, elo, i, j, el, n1, n2, n3, n4;

	int i;
	int el;
	double area, dX[4], weigh1, weigh2, weigh3, weigh4, weigh5, weigh6, weigh7, weigh8;

	//static int onf = 0;
	static int been_here = 0;
	static double dx, dy;

	if(been_here++ == 0)
	{
		dx = (E->XP[1][E->lmesh.nox] - E->XP[1][1]) / E->lmesh.elx;
		dy = (E->XP[2][E->lmesh.noy] - E->XP[2][1]) / E->lmesh.ely;
	}

/*	sprintf(filename1,"markers%d.%d",E->advection.timesteps,onf);
	fp0 = fopen(filename1,"w"); 
	onf = (onf == 0) ? 1 : 0;
*/

/*  el can also be obtained from CElement[i] and dX can then be
 dX[1]=XMC[1][i]-X[1][ien[el].node[1]]
and  dX[2]=XMC[2][i]-X[2][ien[el].node[1]]. So no element number
is needed to be sought. But since it is so easy to get anyway
for 2D, we do not want to implement this yet
*/

	if(con == 1)
	{
		for(i = 1; i <= E->advection.markers; i++)
		{

			el = get_element(E, E->XMCpred[1][i], E->XMCpred[2][i], E->XMCpred[3][i], dX);

			weigh1 = (dx - dX[1]) * (dy - dX[2]) * (E->eco[el].size[3] - dX[3]);
			weigh2 = dX[1] * (dy - dX[2]) * (E->eco[el].size[3] - dX[3]);
			weigh3 = dX[1] * dX[2] * (E->eco[el].size[3] - dX[3]);
			weigh4 = (dx - dX[1]) * dX[2] * (E->eco[el].size[3] - dX[3]);
			weigh5 = (dx - dX[1]) * (dy - dX[2]) * dX[3];
			weigh6 = dX[1] * (dy - dX[2]) * dX[3];
			weigh7 = dX[1] * dX[2] * dX[3];
			weigh8 = (dx - dX[1]) * dX[2] * dX[3];

			area = dx * dy * E->eco[el].size[3];

			E->Vpred[1][i] = (weigh1 * V[1][E->ien[el].node[1]] + weigh2 * V[1][E->ien[el].node[2]] + weigh3 * V[1][E->ien[el].node[3]] + weigh4 * V[1][E->ien[el].node[4]] + weigh5 * V[1][E->ien[el].node[5]] + weigh6 * V[1][E->ien[el].node[6]] + weigh7 * V[1][E->ien[el].node[7]] + weigh8 * V[1][E->ien[el].node[8]]) / area;
			E->Vpred[2][i] = (weigh1 * V[2][E->ien[el].node[1]] + weigh2 * V[2][E->ien[el].node[2]] + weigh3 * V[2][E->ien[el].node[3]] + weigh4 * V[2][E->ien[el].node[4]] + weigh5 * V[2][E->ien[el].node[5]] + weigh6 * V[2][E->ien[el].node[6]] + weigh7 * V[2][E->ien[el].node[7]] + weigh8 * V[2][E->ien[el].node[8]]) / area;
			E->Vpred[3][i] = (weigh1 * V[3][E->ien[el].node[1]] + weigh2 * V[3][E->ien[el].node[2]] + weigh3 * V[3][E->ien[el].node[3]] + weigh4 * V[3][E->ien[el].node[4]] + weigh5 * V[3][E->ien[el].node[5]] + weigh6 * V[3][E->ien[el].node[6]] + weigh7 * V[3][E->ien[el].node[7]] + weigh8 * V[3][E->ien[el].node[8]]) / area;

			E->CElement[i] = el;

		}
	}
	else if(con == 0)
	{
		for(i = 1; i <= E->advection.markers; i++)
		{

			el = get_element(E, E->XMC[1][i], E->XMC[2][i], E->XMC[3][i], dX);

			weigh1 = (dx - dX[1]) * (dy - dX[2]) * (E->eco[el].size[3] - dX[3]);
			weigh2 = dX[1] * (dy - dX[2]) * (E->eco[el].size[3] - dX[3]);
			weigh3 = dX[1] * dX[2] * (E->eco[el].size[3] - dX[3]);
			weigh4 = (dx - dX[1]) * dX[2] * (E->eco[el].size[3] - dX[3]);
			weigh5 = (dx - dX[1]) * (dy - dX[2]) * dX[3];
			weigh6 = dX[1] * (dy - dX[2]) * dX[3];
			weigh7 = dX[1] * dX[2] * dX[3];
			weigh8 = (dx - dX[1]) * dX[2] * dX[3];

			area = dx * dy * E->eco[el].size[3];

			E->VO[1][i] = (weigh1 * V[1][E->ien[el].node[1]] + weigh2 * V[1][E->ien[el].node[2]] + weigh3 * V[1][E->ien[el].node[3]] + weigh4 * V[1][E->ien[el].node[4]] + weigh5 * V[1][E->ien[el].node[5]] + weigh6 * V[1][E->ien[el].node[6]] + weigh7 * V[1][E->ien[el].node[7]] + weigh8 * V[1][E->ien[el].node[8]]) / area;
			E->VO[2][i] = (weigh1 * V[2][E->ien[el].node[1]] + weigh2 * V[2][E->ien[el].node[2]] + weigh3 * V[2][E->ien[el].node[3]] + weigh4 * V[2][E->ien[el].node[4]] + weigh5 * V[2][E->ien[el].node[5]] + weigh6 * V[2][E->ien[el].node[6]] + weigh7 * V[2][E->ien[el].node[7]] + weigh8 * V[2][E->ien[el].node[8]]) / area;
			E->VO[3][i] = (weigh1 * V[3][E->ien[el].node[1]] + weigh2 * V[3][E->ien[el].node[2]] + weigh3 * V[3][E->ien[el].node[3]] + weigh4 * V[3][E->ien[el].node[4]] + weigh5 * V[3][E->ien[el].node[5]] + weigh6 * V[3][E->ien[el].node[6]] + weigh7 * V[3][E->ien[el].node[7]] + weigh8 * V[3][E->ien[el].node[8]]) / area;

			E->CElement[i] = el;


		}
	}

	return;
}

/* ================================================ 
  works for uniform mesh in x and y, but unlimited in z

  (has to be in the right processor)

 ================================================ */

int get_element(struct All_variables *E, CITCOM_XMC_PREC XMC1, CITCOM_XMC_PREC XMC2, CITCOM_XMC_PREC XMC3, double dX[4])
{
  int el;
  int i, i1, j1;
  //int done, i, i1, i2, ii, j, j1, j2, jj, el;
  
  const int nox = E->lmesh.nox;
  const int noy = E->lmesh.noy;
  const int noz = E->lmesh.noz;
  const int elx = E->lmesh.elx;
  const int ely = E->lmesh.ely;
  const int elz = E->lmesh.elz;
  const int elz_elx =  E->lmesh.elz * E->lmesh.elx;
  static int been_here = 0;
  static double dx, dy, dzz;
#ifdef DEBUG
  i = locate_processor(E,XMC1, XMC2,XMC3);
  if(i != E->parallel.me){
    fprintf(stderr,"cpu %i: tracer at %g %g %g should be in processor %i\n",
	    E->parallel.me, XMC1,XMC2,XMC3,i);
    myerror("wrong processor error in get_element",E);
  }
#endif 
  if(been_here++ == 0){

    dx = (E->XP[1][nox] - E->XP[1][1]) / elx;
    dy = (E->XP[2][noy] - E->XP[2][1]) / ely;
    dzz = (E->XP[3][noz] - E->XP[3][1]) / (E->lmesh.rnoz - 1);

    for(i = 1; i <= nox; i++)
      fprintf(E->fp, "%lf\n", E->XP[1][i]);

    for(i = 1; i <= noy; i++)
      fprintf(E->fp, "%lf\n", E->XP[2][i]);
    
    for(i = 1; i <= noz; i++)
      fprintf(E->fp, "%lf\n", E->XP[3][i]);

    fprintf(E->fp, "%lf %lf\n", E->XG1[1], E->XG2[1]);
    fprintf(E->fp, "%lf %lf\n", E->XG1[2], E->XG2[2]);
    fprintf(E->fp, "%lf %lf\n", E->XG1[3], E->XG2[3]);
    fflush(E->fp);
  }
#ifdef DEBUG
  if((XMC1 < E->XP[1][1]) || (XMC1 > E->XP[1][nox]) || 
     (XMC2 < E->XP[2][1]) || (XMC2 > E->XP[2][noy]) || 
     (XMC3 < E->XP[3][1]) || (XMC3 > E->XP[3][noz])){
    fprintf(stderr,"cpu %i: x %g < %g < %g y %g < %g < %g z %g < %g < %g \n",
	    E->parallel.me,
	    E->XP[1][1],XMC1,E->XP[1][nox],
	    E->XP[2][1],XMC2,E->XP[2][noy],
	    E->XP[3][1],XMC3,E->XP[3][noz]);
    myerror("out of bounds error 1 in get_element",E);
  }
#endif
 

  E->advection.markerIX = min((XMC1 - E->XP[1][1]) / dx + 1, elx);
  dX[1] = XMC1 - E->XP[1][E->advection.markerIX];
  E->advection.markerIY = min((XMC2 - E->XP[2][1]) / dy + 1, ely);
  dX[2] = XMC2 - E->XP[2][E->advection.markerIY];
  
  i1 = min((XMC3 - E->XP[3][1]) / dzz + 1, E->lmesh.rnoz - 1);
  E->advection.markerIZ = E->RG[3][i1];
  
  if(E->advection.markerIZ){
    dX[3] = XMC3 - E->XP[3][E->advection.markerIZ];
  }else{
    for(i = 0; i <= 2; i = i + 2){
      j1 = E->RG[3][i1 - 1 + i];
      if(XMC3 >= E->XP[3][j1] && XMC3 <= E->XP[3][j1 + 1]){
	E->advection.markerIZ = j1;
	dX[3] = XMC3 - E->XP[3][E->advection.markerIZ];
      }
    }
  }

  el = E->advection.markerIZ + (E->advection.markerIX - 1) * elz + (E->advection.markerIY - 1) * elz_elx;
#ifdef DEBUG
  if((el<1) || (el > E->lmesh.nel)){
    fprintf(stderr,"cpu %i i: %i %i %i bounds: %i %i %i total %i el %i\n",
	    E->parallel.me,E->advection.markerIX ,E->advection.markerIY,E->advection.markerIZ,
	    E->lmesh.elx,E->lmesh.ely,E->lmesh.elz,
	    E->lmesh.nel,el);
    myerror("out of bounds error 2 in get_element",E);
  }
#endif

  if(!E->advection.markerIZ){
    fprintf(E->fp, "!!!overflow %g %g %g %d\n", XMC1, XMC2, XMC3, el);
    fflush(E->fp);
    parallel_process_termination();
  }
  
  return (el);
}


int in_the_domain(struct All_variables *E, double r, double t, double f)
{
	int done;
	const int nno = E->lmesh.nno;

	done = 0;
	if(r > E->SX[3][nno] || r < E->SX[3][1])
		return (done);

	if(t > E->SX[1][nno] || t < E->SX[1][1])
		return (done);

	if(f > E->SX[2][nno] || f < E->SX[2][1])
		return (done);

	done = 1;

	return (done);
}

/* ====================================================  */

/* ============================================= */

float area_of_4node1(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4)
{
	float area = 0.0;
	float temp1, temp2;

	temp1 = 0.5 * (x1 + x2);
	temp2 = 0.5 * (y1 + y2);

	if(fabs(x1 - x2) == 2.0)
		area = 2.0 * fabs(temp2 - y3);
	else if(fabs(y1 - y2) == 2.0)
		area = 2.0 * fabs(temp1 - x3);

	return area;
}

/* ============================================= */

float area_of_3node(float x1, float y1, float x2, float y2, float x3, float y3)
{
	float area;

	area = 0.5 * max(fabs(y3 - y1), fabs(y3 - y2)) * max(fabs(x3 - x1), fabs(x3 - x2));

	return area;
}


/* ============================================= */

float mean_of_3node(int a, float x1, float y1, float x2, float y2, float x3, float y3)
{
	float mean, xm, ym;

	xm = (x1 + x2 + x3) / 3.0;
	ym = (y1 + y2 + y3) / 3.0;

	mean = 0.25 * (1.0 + xxsh[a][1] * xm) * (1.0 + xxsh[a][2] * ym);

	return mean;
}

/* ============================================= */

float mean_of_4node(int a, float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4)
{
	float mean, xm, ym;

	xm = (x1 + x2 + x3 + x4) * 0.25;
	ym = (y1 + y2 + y3 + y4) * 0.25;

	mean = 0.25 * (1.0 + xxsh[a][1] * xm) * (1.0 + xxsh[a][2] * ym);

	return mean;
}

/* ============================================= */

float mean_of_5node(int a, float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, float x5, float y5)
{

	float mean, xm, ym;

	xm = (x1 + x2 + x3 + x4 + x5) * 0.2;
	ym = (y1 + y2 + y3 + y4 + y5) * 0.2;

	mean = 0.25 * (1.0 + xxsh[a][1] * xm) * (1.0 + xxsh[a][2] * ym);


	return mean;
}

/* ================================================ */

float dist1(float XO[4], float XN[4])
{

	float dist2;

	dist2 = sqrt((XO[1] - XN[1]) * (XO[1] - XN[1]) + (XO[2] - XN[2]) * (XO[2] - XN[2]));

	return (dist2);
}
