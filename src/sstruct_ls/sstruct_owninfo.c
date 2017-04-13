/*BHEADER**********************************************************************
 * Copyright (c) 2008,  Lawrence Livermore National Security, LLC.
 * Produced at the Lawrence Livermore National Laboratory.
 * This file is part of HYPRE.  See file COPYRIGHT for details.
 *
 * HYPRE is free software; you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License (as published by the Free
 * Software Foundation) version 2.1 dated February 1999.
 *
 * $Revision$
 ***********************************************************************EHEADER*/

#include "_hypre_sstruct_ls.h"

HYPRE_Int
hypre_SStructIndexScaleF_C( hypre_Index findex,
                            hypre_Index index,
                            hypre_Index stride,
                            hypre_Index cindex )
{
   HYPRE_Int d;
 
   for (d = 0; d < HYPRE_MAXDIM; d++)
   {
      hypre_IndexD(cindex,d) =
         (hypre_IndexD(findex,d) - hypre_IndexD(index,d)) / hypre_IndexD(stride,d);
   }

   return 0;
}


HYPRE_Int
hypre_SStructIndexScaleC_F( hypre_Index cindex,
                            hypre_Index index,
                            hypre_Index stride,
                            hypre_Index findex )
{
   HYPRE_Int d;
 
   for (d = 0; d < HYPRE_MAXDIM; d++)
   {
      hypre_IndexD(findex,d) =
         hypre_IndexD(cindex,d) * hypre_IndexD(stride,d) + hypre_IndexD(index,d);
   }

   return 0;
}
/*--------------------------------------------------------------------------
 * hypre_SStructOwnInfo: Given a fgrid, coarsen each fbox and find the
 * coarsened boxes that belong on my current processor. These are my own_boxes.
 *--------------------------------------------------------------------------*/

hypre_SStructOwnInfoData *
hypre_SStructOwnInfo( hypre_StructGrid  *fgrid,
                      hypre_StructGrid  *cgrid,
                      hypre_BoxManager  *cboxman,
                      hypre_BoxManager  *fboxman,
                      hypre_Index        rfactor )
{
   hypre_SStructOwnInfoData *owninfo_data;

   MPI_Comm                  comm= hypre_SStructVectorComm(fgrid);
   HYPRE_Int                 ndim= hypre_StructGridNDim(fgrid);

   hypre_BoxArray           *grid_boxes;
   hypre_BoxArray           *intersect_boxes;
   hypre_BoxArray           *tmp_boxarray;

   hypre_Box                *grid_box, scaled_box;
   hypre_Box                 boxman_entry_box;

   hypre_BoxManEntry       **boxman_entries;
   HYPRE_Int                 nboxman_entries;

   hypre_BoxArrayArray      *own_boxes;
   HYPRE_Int               **own_cboxnums;

   hypre_BoxArrayArray      *own_composite_cboxes;

   hypre_Index               ilower, iupper, index;

   HYPRE_Int                 myproc, proc;

   HYPRE_Int                 cnt;
   HYPRE_Int                 i, j, k, mod;

   hypre_BoxInit(&scaled_box, ndim);
   hypre_BoxInit(&boxman_entry_box, ndim);

   hypre_ClearIndex(index); 
   hypre_MPI_Comm_rank(comm, &myproc);

   owninfo_data= hypre_CTAlloc(hypre_SStructOwnInfoData, 1);

   /*------------------------------------------------------------------------
    * Create the structured ownbox patterns. 
    *
    *   own_boxes are obtained by intersecting this proc's fgrid boxes
    *   with cgrid's box_man. Intersecting BoxManEntries on this proc
    *   will give the own_boxes.
    *------------------------------------------------------------------------*/
   grid_boxes    = hypre_StructGridBoxes(fgrid);

   own_boxes   = hypre_BoxArrayArrayCreate(hypre_BoxArraySize(grid_boxes), ndim);
   own_cboxnums= hypre_CTAlloc(HYPRE_Int *, hypre_BoxArraySize(grid_boxes));

   hypre_ForBoxI(i, grid_boxes)
   {
       grid_box= hypre_BoxArrayBox(grid_boxes, i);

       /*---------------------------------------------------------------------
        * Find the boxarray that is owned. BoxManIntersect returns
        * the full extents of the boxes that intersect with the given box.
        * We further need to intersect each box in the list with the given
        * box to determine the actual box that is owned.
        *---------------------------------------------------------------------*/
       hypre_SStructIndexScaleF_C(hypre_BoxIMin(grid_box), index,
                                  rfactor, hypre_BoxIMin(&scaled_box));
       hypre_SStructIndexScaleF_C(hypre_BoxIMax(grid_box), index,
                                  rfactor, hypre_BoxIMax(&scaled_box));

       hypre_BoxManIntersect(cboxman, hypre_BoxIMin(&scaled_box), 
                             hypre_BoxIMax(&scaled_box), &boxman_entries,
                             &nboxman_entries);

       cnt= 0;
       for (j= 0; j< nboxman_entries; j++)
       {
          hypre_SStructBoxManEntryGetProcess(boxman_entries[j], &proc);
          if (proc == myproc)
          {
             cnt++;
          }
       }
       own_cboxnums[i]= hypre_CTAlloc(HYPRE_Int, cnt);

       cnt= 0;
       for (j= 0; j< nboxman_entries; j++)
       {
          hypre_SStructBoxManEntryGetProcess(boxman_entries[j], &proc);

          /* determine the chunk of the boxman_entries[j] box that is needed */
          hypre_BoxManEntryGetExtents(boxman_entries[j], ilower, iupper);
          hypre_BoxSetExtents(&boxman_entry_box, ilower, iupper);
          hypre_IntersectBoxes(&boxman_entry_box, &scaled_box, &boxman_entry_box);

          if (proc == myproc)
          {
             hypre_SStructBoxManEntryGetBoxnum(boxman_entries[j], &own_cboxnums[i][cnt]);
             hypre_AppendBox(&boxman_entry_box, 
                             hypre_BoxArrayArrayBoxArray(own_boxes, i));
             cnt++;
          }
      } 
      hypre_TFree(boxman_entries);
   }  /* hypre_ForBoxI(i, grid_boxes) */ 

   (owninfo_data -> size)     = hypre_BoxArraySize(grid_boxes);
   (owninfo_data -> own_boxes)= own_boxes;
   (owninfo_data -> own_cboxnums)= own_cboxnums;

   /*------------------------------------------------------------------------
    *   own_composite_cboxes are obtained by intersecting this proc's cgrid 
    *   boxes with fgrid's box_man. For each cbox, subtracting all the 
    *   intersecting boxes from all processors will give the 
    *   own_composite_cboxes.
    *------------------------------------------------------------------------*/
   grid_boxes= hypre_StructGridBoxes(cgrid);
   own_composite_cboxes= hypre_BoxArrayArrayCreate(hypre_BoxArraySize(grid_boxes), ndim);
  (owninfo_data -> own_composite_size)= hypre_BoxArraySize(grid_boxes);

   tmp_boxarray = hypre_BoxArrayCreate(0, ndim);
   hypre_ForBoxI(i, grid_boxes)
   {
       grid_box= hypre_BoxArrayBox(grid_boxes, i);
       hypre_AppendBox(grid_box,
                       hypre_BoxArrayArrayBoxArray(own_composite_cboxes, i));

       hypre_ClearIndex(index); 
       hypre_SStructIndexScaleC_F(hypre_BoxIMin(grid_box), index,
                                  rfactor, hypre_BoxIMin(&scaled_box));
       for (j = 0; j < HYPRE_MAXDIM; j++) { hypre_IndexD(index,j) = rfactor[j]-1; }
       hypre_SStructIndexScaleC_F(hypre_BoxIMax(grid_box), index,
                                  rfactor, hypre_BoxIMax(&scaled_box));

       hypre_BoxManIntersect(fboxman, hypre_BoxIMin(&scaled_box),
                             hypre_BoxIMax(&scaled_box), &boxman_entries,
                            &nboxman_entries);
       
       hypre_ClearIndex(index); 
       intersect_boxes= hypre_BoxArrayCreate(0, ndim);
       for (j= 0; j< nboxman_entries; j++)
       {
          hypre_BoxManEntryGetExtents(boxman_entries[j], ilower, iupper);
          hypre_BoxSetExtents(&boxman_entry_box, ilower, iupper);
          hypre_IntersectBoxes(&boxman_entry_box, &scaled_box, &boxman_entry_box);

         /* contract the intersection box so that only the cnodes in the 
            intersection box are included. */
          for (k= 0; k< ndim; k++)
          {
             mod= hypre_BoxIMin(&boxman_entry_box)[k] % rfactor[k];
             if (mod)
             {
                hypre_BoxIMin(&boxman_entry_box)[k]+= rfactor[k] - mod;
             }
          }
 
          hypre_SStructIndexScaleF_C(hypre_BoxIMin(&boxman_entry_box), index,
                                     rfactor, hypre_BoxIMin(&boxman_entry_box));
          hypre_SStructIndexScaleF_C(hypre_BoxIMax(&boxman_entry_box), index,
                                     rfactor, hypre_BoxIMax(&boxman_entry_box));
          hypre_AppendBox(&boxman_entry_box, intersect_boxes);
       }

       hypre_SubtractBoxArrays(hypre_BoxArrayArrayBoxArray(own_composite_cboxes, i),
                               intersect_boxes, tmp_boxarray);
       hypre_MinUnionBoxes(hypre_BoxArrayArrayBoxArray(own_composite_cboxes, i));

       hypre_TFree(boxman_entries);
       hypre_BoxArrayDestroy(intersect_boxes);
   }
   hypre_BoxArrayDestroy(tmp_boxarray);
       
  (owninfo_data -> own_composite_cboxes)= own_composite_cboxes;

   return owninfo_data;
}

/*--------------------------------------------------------------------------
 * hypre_SStructOwnInfoDataDestroy
 *--------------------------------------------------------------------------*/
HYPRE_Int
hypre_SStructOwnInfoDataDestroy(hypre_SStructOwnInfoData *owninfo_data)
{
   HYPRE_Int ierr = 0;
   HYPRE_Int i;

   if (owninfo_data)
   {
      if (owninfo_data -> own_boxes)
      {
         hypre_BoxArrayArrayDestroy( (owninfo_data -> own_boxes) );
      }

      for (i= 0; i< (owninfo_data -> size); i++)
      {
         if (owninfo_data -> own_cboxnums[i])
         {
             hypre_TFree(owninfo_data -> own_cboxnums[i]);
         }
      }
      hypre_TFree(owninfo_data -> own_cboxnums);

      if (owninfo_data -> own_composite_cboxes)
      {
         hypre_BoxArrayArrayDestroy( (owninfo_data -> own_composite_cboxes) );
      }
   }

   hypre_TFree(owninfo_data);

   return ierr;
}
