#ifndef __DSK_AVLHTREE_MACROS_H_
#define __DSK_AVLHTREE_MACROS_H_

/* Macros for construction of a red-black tree.
 * Like our other macro-based data-structures,
 * this doesn't allocate memory, and it doesn't
 * use any helper functions.
 *
 * It supports "invasive" tree structures,
 * for example, you can have nodes that are members
 * of two different trees.
 *
 * An avlhtree is a tuple:
 *    top
 *    type*
 *    left
 *    right
 *    parent
 *    height
 *    comparator
 * that should be defined by a macro like "GET_TREE()".
 * See tests/test-rbtree-macros.
 *
 */

#define DSK_AVLHTREE_INSERT_AT_(top,type,left,right,parent,balance,comparator, parent_node, is_right, new_node) \
 do{ \
   type _dsk_parent = parent;
   type _dsk_at = new_node;
   type _dsk_is_right = is_right;
   for (;;)
     {
       int delta_balance = is_right ? 1 : -1;
       int new_balance = delta_balance + _dsk_parent->balance;
       switch (new_balance)
         {
         case -2:
         case -1:
         case 0:
         case 1:
         case 2:
         }
       if (new_balance == -2)
         {
         }
       else if (new_balance == 2)
         {
         }
       else
         {
           _dsk_parent->balance = new_balance;
           return
         }
  parent
   

