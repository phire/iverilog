/*
 * Copyright (c) 2005 Stephen Williams (steve@icarus.com)
 *
 * (This is a rewrite of code that was ...
 * Copyright (c) 2001 Stephan Boettcher <stephan@nevis.columbia.edu>)
 *
 *    This source code is free software; you can redistribute it
 *    and/or modify it in source code form under the terms of the GNU
 *    General Public License as published by the Free Software
 *    Foundation; either version 2 of the License, or (at your option)
 *    any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */
#ifdef HAVE_CVS_IDENT
#ident "$Id: udp.cc,v 1.30 2005/06/09 04:12:30 steve Exp $"
#endif

#include "udp.h"
#include "schedule.h"
#include "symbols.h"
#include "compile.h"
#include <assert.h>
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <iostream>

static symbol_table_t udp_table;

struct vvp_udp_s *udp_find(const char *label)
{
      symbol_value_t v = sym_get_value(udp_table, label);
      return (struct vvp_udp_s *)v.ptr;
}

vvp_udp_s::vvp_udp_s(char*label, unsigned ports)
: ports_(ports)
{
      if (!udp_table)
	    udp_table = new_symbol_table();

      assert( !udp_find(label) );

      symbol_value_t v;
      v.ptr = this;
      sym_set_value(udp_table, label, v);
}

vvp_udp_s::~vvp_udp_s()
{
}

unsigned vvp_udp_s::port_count() const
{
      return ports_;
}

vvp_udp_comb_s::vvp_udp_comb_s(char*label, char*name, unsigned ports)
: vvp_udp_s(label, ports)
{
      name_ = name;
      levels0_ = 0;
      levels1_ = 0;
      nlevels0_ = 0;
      nlevels1_ = 0;
}

vvp_udp_comb_s::~vvp_udp_comb_s()
{
      if (levels0_) delete[] levels0_;
      if (levels1_) delete[] levels1_;
}

/*
 * The cur table that is passed in must have for every valid bit
 * position exactly one of the three mask bits set. This represents an
 * actual vector of inputs to be tested.
 *
 * The levels0_ and levels1_ tables have levels_table objects that
 * eack represent a single row. For the row to match the input vector,
 * all the bits that are set in the cur table must also be set in the
 * row being tested.
 *
 * It is possible for a row to match multiple different vectors. This
 * is seen from the compile_table function, where bit positions for
 * multiple masks can be test for certain row positions. For example,
 * if the row bit position is '?', then mask 0/1/x are all set in the
 * row for that bit position. This means it doesn't matter which of
 * the three bit positions is set in the cur input table, the bit
 * position will generate a match.
 */
vvp_bit4_t vvp_udp_comb_s::test_levels(const udp_levels_table&cur)
{
	/* To test for a row match, test that the mask0, mask1 and
	   maskx vectors all have bits set where the matching
	   cur.mask0/1/x vectors have bits set. It is possible that a
	   levels0_[idx] vector has more bits set then the cur mask,
	   but this is OK and these bits are to be ignored. */

      for (unsigned idx = 0 ;  idx < nlevels0_ ;  idx += 1) {
	    if (cur.mask0 != (cur.mask0 & levels0_[idx].mask0))
		  continue;
	    if (cur.mask1 != (cur.mask1 & levels0_[idx].mask1))
		  continue;
	    if (cur.maskx != (cur.maskx & levels0_[idx].maskx))
		  continue;

	    return BIT4_0;
      }

      for (unsigned idx = 0 ;  idx < nlevels1_ ;  idx += 1) {
	    if (cur.mask0 != (cur.mask0 & levels1_[idx].mask0))
		  continue;
	    if (cur.mask1 != (cur.mask1 & levels1_[idx].mask1))
		  continue;
	    if (cur.maskx != (cur.maskx & levels1_[idx].maskx))
		  continue;

	    return BIT4_1;
      }

      return BIT4_X;
}

vvp_bit4_t vvp_udp_comb_s::calculate_output(const udp_levels_table&cur,
					    const udp_levels_table&,
					    vvp_bit4_t)
{
      return test_levels(cur);
}

static void or_based_on_char(udp_levels_table&cur, char flag,
			     unsigned long mask_bit)
{
      switch (flag) {
	  case '0':
	    cur.mask0 |= mask_bit;
	    break;
	  case '1':
	    cur.mask1 |= mask_bit;
	    break;
	  case 'x':
	    cur.maskx |= mask_bit;
	    break;
	  case 'b':
	    cur.mask0 |= mask_bit;
	    cur.mask1 |= mask_bit;
	    break;
	  case 'l':
	    cur.mask0 |= mask_bit;
	    cur.maskx |= mask_bit;
	    break;
	  case 'h':
	    cur.maskx |= mask_bit;
	    cur.mask1 |= mask_bit;
	    break;
	  case '?':
	    cur.mask0 |= mask_bit;
	    cur.maskx |= mask_bit;
	    cur.mask1 |= mask_bit;
	    break;
	  default:
	    assert(0);
      }
}

void vvp_udp_comb_s::compile_table(char**tab)
{
      unsigned nrows0 = 0, nrows1 = 0;

	/* First run through the table to figure out the number of
	   rows I need for each kind of table. */
      for (unsigned idx = 0 ;  tab[idx] ;  idx += 1) {
	    assert(strlen(tab[idx]) == port_count() + 1);
	    switch (tab[idx][port_count()]) {
		case '0':
		  nrows0 += 1;
		  break;
		case '1':
		  nrows1 += 1;
		  break;
		case 'x':
		  break;
		default:
		  assert(0);
	    }
      }

      nlevels0_ = nrows0;
      levels0_ = new udp_levels_table[nlevels0_];

      nlevels1_ = nrows1;
      levels1_ = new udp_levels_table[nlevels1_];

      nrows0 = 0;
      nrows1 = 0;
      for (unsigned idx = 0 ;  tab[idx] ;  idx += 1) {
	    struct udp_levels_table cur;
	    cur.mask0 = 0;
	    cur.mask1 = 0;
	    cur.maskx = 0;
	    assert(port_count() <= sizeof(cur.mask0));
	    for (unsigned pp = 0 ;  pp < port_count() ;  pp += 1) {
		  unsigned long mask_bit = 1UL << pp;
		  or_based_on_char(cur, tab[idx][pp], mask_bit);
	    }

	    switch (tab[idx][port_count()]) {
		case '0':
		  levels0_[nrows0++] = cur;
		  break;
		case '1':
		  levels1_[nrows1++] = cur;
		  break;
		default:
		  break;
	    }
      }

      assert(nrows0 == nlevels0_);
      assert(nrows1 == nlevels1_);
}

vvp_udp_seq_s::vvp_udp_seq_s(char*label, char*name, unsigned ports)
: vvp_udp_s(label, ports)
{
      levels0_ = 0;
      levels1_ = 0;
      levelsx_ = 0;
      levelsL_ = 0;

      nlevels0_ = 0;
      nlevels1_ = 0;
      nlevelsx_ = 0;
      nlevelsL_ = 0;

      edges0_ = 0;
      edges1_ = 0;
      edgesL_ = 0;

      nedges0_ = 0;
      nedges1_ = 0;
      nedgesL_ = 0;
}

vvp_udp_seq_s::~vvp_udp_seq_s()
{
      if (levels0_) delete[]levels0_;
      if (levels1_) delete[]levels1_;
      if (levelsx_) delete[]levelsx_;
      if (levelsL_) delete[]levelsL_;
      if (edges0_)  delete[]edges0_;
      if (edges1_)  delete[]edges1_;
      if (edgesL_)  delete[]edgesL_;
}

void edge_based_on_char(struct udp_edges_table&cur, char chr, unsigned pos)
{
      unsigned long mask_bit = 1 << pos;

      switch (chr) {
	  case '0':
	    cur.mask0 |= mask_bit;
	    break;
	  case '1':
	    cur.mask1 |= mask_bit;
	    break;
	  case 'x':
	    cur.maskx |= mask_bit;
	    break;
	  case 'b':
	    cur.mask0 |= mask_bit;
	    cur.mask1 |= mask_bit;
	    break;
	  case 'l':
	    cur.mask0 |= mask_bit;
	    cur.maskx |= mask_bit;
	    break;
	  case 'h':
	    cur.maskx |= mask_bit;
	    cur.mask1 |= mask_bit;
	    break;
	  case '?':
	    cur.mask0 |= mask_bit;
	    cur.maskx |= mask_bit;
	    cur.mask1 |= mask_bit;
	    break;

	  case 'f': // (10) edge
	    cur.mask0 |= mask_bit;
	    cur.edge_position = pos;
	    cur.edge_mask0 = 0;
	    cur.edge_maskx = 0;
	    cur.edge_mask1 = 1;
	    break;
	  case 'q': // (bx) edge
	    cur.maskx |= mask_bit;
	    cur.edge_position = pos;
	    cur.edge_mask0 = 1;
	    cur.edge_maskx = 0;
	    cur.edge_mask1 = 1;
	    break;
	  case 'r': // (01) edge
	    cur.mask1 |= mask_bit;
	    cur.edge_position = pos;
	    cur.edge_mask0 = 1;
	    cur.edge_maskx = 0;
	    cur.edge_mask1 = 0;
	    break;
	  default:
	    assert(0);
      }
}

void vvp_udp_seq_s::compile_table(char**tab)
{

      for (unsigned idx = 0 ;  tab[idx] ;  idx += 1) {
	    const char*row = tab[idx];
	    assert(strlen(row) == port_count() + 2);

	    if (strspn(row, "01xblh?") >= port_count()+1) {

		  switch (row[port_count()+1]) {
		      case '0':
			nlevels0_ += 1;
			break;
		      case '1':
			nlevels1_ += 1;
			break;
		      case 'x':
			nlevelsx_ += 1;
			break;
		      case '-':
			nlevelsL_ += 1;
			break;
		      default:
			assert(0);
			break;
		  }

	    } else {

		  switch (row[port_count()+1]) {
		      case '0':
			nedges0_ += 1;
			break;
		      case '1':
			nedges1_ += 1;
			break;
		      case 'x':
			break;
		      case '-':
			nedgesL_ += 1;
			break;
		      default:
			assert(0);
			break;
		  }
	    }
      }

      levels0_ = new udp_levels_table[nlevels0_];
      levels1_ = new udp_levels_table[nlevels1_];
      levelsx_ = new udp_levels_table[nlevelsx_];
      levelsL_ = new udp_levels_table[nlevelsL_];
      edges0_ = new udp_edges_table[nedges0_];
      edges1_ = new udp_edges_table[nedges1_];
      edgesL_ = new udp_edges_table[nedgesL_];

      unsigned idx_lev0 = 0;
      unsigned idx_lev1 = 0;
      unsigned idx_levx = 0;
      unsigned idx_levL = 0;
      unsigned idx_edg0 = 0;
      unsigned idx_edg1 = 0;
      unsigned idx_edgL = 0;

      for (unsigned idx = 0 ;  tab[idx] ;  idx += 1) {
	    const char*row = tab[idx];

	    if (strspn(row, "01xblh?") >= port_count()+1) {
		  struct udp_levels_table cur;
		  cur.mask0 = 0;
		  cur.mask1 = 0;
		  cur.maskx = 0;
		  for (unsigned pp = 0 ;  pp < port_count() ;  pp += 1) {
			unsigned long mask_bit = 1UL << pp;
			or_based_on_char(cur, row[pp+1], mask_bit);
		  }

		  or_based_on_char(cur, row[0], 1UL << port_count());

		  switch (row[port_count()+1]) {
		      case '0':
			levels0_[idx_lev0++] = cur;
			break;
		      case '1':
			levels1_[idx_lev1++] = cur;
			break;
		      case 'x':
			levelsx_[idx_levx++] = cur;
			break;
		      case '-':
			levelsL_[idx_levL++] = cur;
			break;
		      default:
			assert(0);
			break;
		  }

	    } else {
		  struct udp_edges_table cur;
		  cur.mask0 = 0;
		  cur.mask1 = 0;
		  cur.maskx = 0;
		  cur.edge_position = 0;
		  cur.edge_mask0 = 0;
		  cur.edge_mask1 = 0;
		  cur.edge_maskx = 0;

		  for (unsigned pp = 0 ;  pp < port_count() ; pp += 1) {
			edge_based_on_char(cur, row[pp+1], pp);
		  }
		  edge_based_on_char(cur, row[0], port_count());

		  switch (row[port_count()+1]) {
		      case '0':
			edges0_[idx_edg0++] = cur;
			break;
		      case '1':
			edges1_[idx_edg1++] = cur;
			break;
		      case 'x':
			break;
		      case '-':
			edgesL_[idx_edgL++] = cur;
			break;
		      default:
			assert(0);
			break;
		  }

	    }
      }
}

vvp_bit4_t vvp_udp_seq_s::calculate_output(const udp_levels_table&cur,
					   const udp_levels_table&prev,
					   vvp_bit4_t cur_out)
{
      udp_levels_table cur_tmp = cur;

      unsigned long mask_out = 1UL << port_count();
      switch (cur_out) {
	  case BIT4_0:
	    cur_tmp.mask0 |= mask_out;
	    break;
	  case BIT4_1:
	    cur_tmp.mask1 |= mask_out;
	    break;
	  default:
	    cur_tmp.maskx |= mask_out;
	    break;
      }

      vvp_bit4_t lev = test_levels_(cur_tmp);
      if (lev == BIT4_Z) {
	    lev = test_edges_(cur_tmp, prev);
      }

      return lev;
}

/*
 * This function tests the levels of the input with the additional
 * check match for the current output. It uses this to calculate a
 * next output, or Z if there was no match. (This is different from
 * the combinational version of this function, which returns X for the
 * cases that don't match.) This method assumes that the caller has
 * set the mask bit in bit postion [port_count()] to reflect the
 * current output.
 */
vvp_bit4_t vvp_udp_seq_s::test_levels_(const udp_levels_table&cur)
{
      for (unsigned idx = 0 ;  idx < nlevels0_ ;  idx += 1) {
	    if (cur.mask0 != (cur.mask0 & levels0_[idx].mask0))
		  continue;
	    if (cur.mask1 != (cur.mask1 & levels0_[idx].mask1))
		  continue;
	    if (cur.maskx != (cur.maskx & levels0_[idx].maskx))
		  continue;

	    return BIT4_0;
      }

      for (unsigned idx = 0 ;  idx < nlevels1_ ;  idx += 1) {
	    if (cur.mask0 != (cur.mask0 & levels1_[idx].mask0))
		  continue;
	    if (cur.mask1 != (cur.mask1 & levels1_[idx].mask1))
		  continue;
	    if (cur.maskx != (cur.maskx & levels1_[idx].maskx))
		  continue;

	    return BIT4_1;
      }

	/* We need to test against an explicit X-output table, since
	   we need to distinguish from an X output and no match. */
      for (unsigned idx = 0 ;  idx < nlevelsx_ ;  idx += 1) {
	    if (cur.mask0 != (cur.mask0 & levelsx_[idx].mask0))
		  continue;
	    if (cur.mask1 != (cur.mask1 & levelsx_[idx].mask1))
		  continue;
	    if (cur.maskx != (cur.maskx & levelsx_[idx].maskx))
		  continue;

	    return BIT4_X;
      }

	/* Test the table that requests the next output be the same as
	   the current output. This gets the current output from the
	   levels table that was passed in. */
      for (unsigned idx = 0 ;  idx < nlevelsL_ ;  idx += 1) {
	    if (cur.mask0 != (cur.mask0 & levelsL_[idx].mask0))
		  continue;
	    if (cur.mask1 != (cur.mask1 & levelsL_[idx].mask1))
		  continue;
	    if (cur.maskx != (cur.maskx & levelsL_[idx].maskx))
		  continue;

	    if (cur.mask0 & (1 << port_count()))
		  return BIT4_0;
	    if (cur.mask1 & (1 << port_count()))
		  return BIT4_1;
	    if (cur.maskx & (1 << port_count()))
		  return BIT4_X;

	    assert(0);
	    return BIT4_X;
      }

	/* No explicit levels entry match. Return a Z to signal that
	   further testing is needed. */
      return BIT4_Z;
}

vvp_bit4_t vvp_udp_seq_s::test_edges_(const udp_levels_table&cur,
				      const udp_levels_table&prev)
{
	/* The edge_mask is true for all bits that are different in
	   the cur and prev tables. */
      unsigned long edge0_mask = cur.mask0 ^ prev.mask0;
      unsigned long edgex_mask = cur.maskx ^ prev.maskx;
      unsigned long edge1_mask = cur.mask1 ^ prev.mask1;

      unsigned long edge_mask = edge0_mask|edgex_mask|edge1_mask;
      edge_mask &= ~ (-1UL << port_count());

	/* If there are no differences, then there are no edges. Give
	   up now. */
      if (edge_mask == 0)
	    return BIT4_X;

      unsigned edge_position = 0;
      while ((edge_mask&1) == 0) {
	    edge_mask >>= 1;
	    edge_position += 1;
      }

	/* We expect that there is exactly one edge in here. */
      assert(edge_mask == 1);

      edge_mask = 1UL << edge_position;

      unsigned edge_mask0 = (prev.mask0&edge_mask)? 1 : 0;
      unsigned edge_maskx = (prev.maskx&edge_mask)? 1 : 0;
      unsigned edge_mask1 = (prev.mask1&edge_mask)? 1 : 0;


	/* Now the edge_position and edge_mask* variables have the
	   values we use to test the applicability of the edge_table
	   entries. */

      for (unsigned idx = 0 ;  idx < nedges0_ ;  idx += 1) {
	    struct udp_edges_table*row = edges0_ + idx;

	    if (row->edge_position != edge_position)
		  continue;
	    if (edge_mask0 && !row->edge_mask0)
		  continue;
	    if (edge_maskx && !row->edge_maskx)
		  continue;
	    if (edge_mask1 && !row->edge_mask1)
		  continue;
	    if (cur.mask0 != (cur.mask0 & row->mask0))
		  continue;
	    if (cur.maskx != (cur.maskx & row->maskx))
		  continue;
	    if (cur.mask1 != (cur.mask1 & row->mask1))
		  continue;

	    return BIT4_0;
      }

      for (unsigned idx = 0 ;  idx < nedges1_ ;  idx += 1) {
	    struct udp_edges_table*row = edges1_ + idx;

	    if (row->edge_position != edge_position)
		  continue;
	    if (edge_mask0 && !row->edge_mask0)
		  continue;
	    if (edge_maskx && !row->edge_maskx)
		  continue;
	    if (edge_mask1 && !row->edge_mask1)
		  continue;
	    if (cur.mask0 != (cur.mask0 & row->mask0))
		  continue;
	    if (cur.maskx != (cur.maskx & row->maskx))
		  continue;
	    if (cur.mask1 != (cur.mask1 & row->mask1))
		  continue;

	    return BIT4_1;
      }

      for (unsigned idx = 0 ;  idx < nedgesL_ ;  idx += 1) {
	    struct udp_edges_table*row = edgesL_ + idx;

	    if (row->edge_position != edge_position)
		  continue;
	    if (edge_mask0 && !row->edge_mask0)
		  continue;
	    if (edge_maskx && !row->edge_maskx)
		  continue;
	    if (edge_mask1 && !row->edge_mask1)
		  continue;
	    if (cur.mask0 != (cur.mask0 & row->mask0))
		  continue;
	    if (cur.maskx != (cur.maskx & row->maskx))
		  continue;
	    if (cur.mask1 != (cur.mask1 & row->mask1))
		  continue;

	    if (cur.mask0 & (1 << port_count()))
		  return BIT4_0;
	    if (cur.mask1 & (1 << port_count()))
		  return BIT4_1;
	    if (cur.maskx & (1 << port_count()))
		  return BIT4_X;

	    assert(0);
	    return BIT4_X;
      }

      return BIT4_X;
}

vvp_udp_fun_core::vvp_udp_fun_core(vvp_net_t*net,
				   vvp_udp_s*def,
				   vvp_delay_t*del)
: vvp_wide_fun_core(net, def->port_count())
{
      def_ = def;
      delay_ = del;
      cur_out_ = BIT4_X;
	// Assume initially that all the inputs are 1'bx
      current_.mask0 = 0;
      current_.mask1 = 0;
      current_.maskx = ~ ((-1UL) << port_count());
}

vvp_udp_fun_core::~vvp_udp_fun_core()
{
}

void vvp_udp_fun_core::recv_vec4_from_inputs(unsigned port)
{
	/* For now, assume udps are 1-bit wide. */
      assert(value(port).size() == 1);

      unsigned long mask = 1UL << port;

      udp_levels_table prev = current_;

      switch (value(port).value(0)) {

	  case BIT4_0:
	    current_.mask0 |= mask;
	    current_.mask1 &= ~mask;
	    current_.maskx &= ~mask;
	    break;
	  case BIT4_1:
	    current_.mask0 &= ~mask;
	    current_.mask1 |= mask;
	    current_.maskx &= ~mask;
	    break;
	  default:
	    current_.mask0 &= ~mask;
	    current_.mask1 &= ~mask;
	    current_.maskx |= mask;
	    break;
      }

      vvp_bit4_t out_bit = def_->calculate_output(current_, prev, cur_out_);
      vvp_vector4_t out (1);
      out.set_bit(0, out_bit);

      if (delay_)
	    propagate_vec4(out, delay_->get_delay(cur_out_, out_bit));
      else
	    propagate_vec4(out);

      cur_out_ = out_bit;
}


/*
 * This function is called by the parser in response to a .udp
 * node. We create the nodes needed to integrate the UDP into the
 * netlist. The definition should be parsed already.
 */
void compile_udp_functor(char*label, char*type,
			 vvp_delay_t*delay,
			 unsigned argc, struct symb_s*argv)
{
      struct vvp_udp_s *def = udp_find(type);
      assert(def);
      free(type);

      vvp_net_t*ptr = new vvp_net_t;
      vvp_udp_fun_core*core = new vvp_udp_fun_core(ptr, def, delay);
      ptr->fun = core;
      define_functor_symbol(label, ptr);
      free(label);

      wide_inputs_connect(core, argc, argv);
      free(argv);
}

/*
 * $Log: udp.cc,v $
 * Revision 1.30  2005/06/09 04:12:30  steve
 *  Support sequential UDP devices.
 *
 * Revision 1.29  2005/04/04 05:13:59  steve
 *  Support row level wildcards.
 *
 * Revision 1.28  2005/04/03 05:45:51  steve
 *  Rework the vvp_delay_t class.
 *
 * Revision 1.27  2005/04/01 06:02:45  steve
 *  Reimplement combinational UDPs.
 *
 */
