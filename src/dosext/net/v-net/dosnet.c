/* 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 1998 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/*
 * DOSNET	A virtual device for usage with dosemu.
 * derived from:
 * INET		An implementation of the TCP/IP protocol suite for the LINUX
 *		operating system.  INET is implemented using the  BSD Socket
 *		interface as the means of communication with the user level.
 *
 *		Pseudo-driver for the dosnet interface.
 *              Based on loopback.c
 *
 * Authors:	Ross Biro, <bir7@leland.Stanford.Edu>
 *		Fred N. van Kempen, <waltje@uWalt.NL.Mugnet.ORG>
 *		Donald Becker, <becker@super.org>
 *
 *		Alan Cox	:	Fixed oddments for NET3.014
 *
 * Changes for dosemu:
 *		Bart Hartgers <barth@stack.nl> : adapt. to Linux-2.0.x
 *              Marcus Better <Marcus.Better@abc.se> :
 *                                      adapted to Linux 2.1.x
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 */

#include <linux/module.h>

#include <linux/socket.h>
#include <linux/in.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>

/* We need this file from the kernel include/net directory
   for the sock structure, unfortunately. */
#include <net/sock.h>

#include "dosnet.h"
#include "kversion.h"

#if LX_KERNEL_VERSION >= 2001000
typedef struct net_device_stats stats_t; /* for Linux 2.1 */
#else
typedef struct enet_statistics stats_t;  /* for earlier kernels */
#endif

unsigned char *dsn_eth_address=DOSNET_DEVICE_ETH_ADDRESS;
unsigned char *dosnet_generic_address=DOSNET_FAKED_ETH_ADDRESS ;

  /* Finding out the "type" of the packet when called by routine
     in dev.c to put it onto proper protocol stack . 
     Gets called ONLY WHEN when a packet is transmitted by dsn0 device.
      
     There are many types of packets which can get generated by dsn0 device. 
     Distinguished by source and destination being all possible combinations
     of  dsn0, dosemu being source or destination. 

     Source      Dest.   Type of packet      Action
  1  dosemu      dsn0    normal              Send with orig. type field      
  2  dsn0        dosemu  normal              Send with dosemu's type field      
  3  dosemu      dosemu  normal              Send with dosemu's type field
  4  dsn0        dsn0    normal              ignore

  Broadcast packets: 
         ***** A packet CAN have ffffffff  as destination. *******
         dsn0 -> dosemu : Not a duplicated packet. But destination address 
                          is changed, so it is covered by one of the above 4 
                          cases.
         dosemu -> dsn0 : Gives rise to a packet with destination ffffffff, and
                          another (duplicated) with dest. generic broadcast
                          address for dosemus. Former gives rise to 4 cases,
                          latter is covered by first 4 cases. 
     source      destination
  5  dsn0        (dsn0)   broadcast           ignore 
  6  dosemu	 ffffffff(dsn0)  broadcast     (duplicated packet) orig type field. 
  7  dosemu	 (dosemu)  broadcast          ffff.. is absent, == case  4.
  8  dsn0        (dosemu)  broadcast           same as 7. 

                Cases                              Strong condition
 Send with original type: cases 1 and 6.        source=dosemu && dest==(dsn0 or ffff). 
 Send with dosemu's type: cases 2, 3.           dest == dosemu.
 Ignore packets:      cases 4,5,7,8 == only 4   source=dsn0 && dest=dsn0.      

 This means, that the source and dest. can only be dsn0, dosemu or broadcast.
 If this is not the case, problems !
*/



/*
 *	Determine the packet's "special" protocol ID if the destination
 *      ethernet addresses start with  precisely chars "db". Protocol
 *      id is then given by the 3rd and 4th fields, and it should not match 
 *      any of existing one.
 *      If this condition is not satisfied, call original eth_type_trans.	
 */
unsigned short int dosnet_eth_type_trans(struct sk_buff *skb, struct device *dev)
{
	int result;
	struct ethhdr *eth = (struct ethhdr *) skb->data;

	result = eth_type_trans(skb,dev);
	eth=skb->mac.ethernet;

        /* This is quite tricky. 
           See the notes in dosnet_xmit.
        */
        /* works properly: tested with proper printks. */ 

        /* cases 2,3: destination is dosnet AND NOT dsn0 ! 
           This is quite sensitive to the addresses assigned to dsn0 and
           to dosemu's. Be sure to double check this!
        */
        if( (eth->h_dest[0] == dosnet_generic_address[0]) &&
            (eth->h_dest[1] == dosnet_generic_address[1]) &&
            (eth->h_dest[3] == dosnet_generic_address[3])   )
        {
              return( htons(*(unsigned short int *)&(eth->h_dest[2])) );
        }
               /* cases  1 and 6.
                  Destination = dsn0 device or ffff is guaranteed.
                  So check only if the Source is dosemu. If not, the source 
                  should be dsn0, and hence ignored.
                */
        else if ( (eth->h_source[0] == dosnet_generic_address[0]) &&
                  (eth->h_source[1] == dosnet_generic_address[1]) &&
                  (eth->h_source[3] == dosnet_generic_address[3])  ) 
        {
	      return result;
        }
        /* cases 4 and 5 are ignored. */
        return( htons(DOSNET_INVALID_TYPE) );
}

static int
dosnet_xmit(struct sk_buff *skb, struct device *dev)
{
	stats_t *stats = (stats_t *)dev->priv;
	struct ethhdr *eth; 
#if LX_KERNEL_VERSION < 2001000
	int unlock=1;
#endif
	
	/* Probably unnecessary */
	if (skb == NULL || dev == NULL) return 0;
	
#if LX_KERNEL_VERSION >= 2001000
	if(atomic_read(&skb->users) != 1)
	{
		/* does this ever happen? */
	  	struct sk_buff *skb2=skb;
	  	skb=skb_clone(skb, GFP_ATOMIC);		/* Clone the buffer */
	  	if(skb==NULL) {
			kfree_skb(skb2);
			return 0;
		}
	  	kfree_skb(skb2);
	}
	else
	{
		/* this is necessary */
		skb_orphan(skb);
	}
#else
	if(skb->free==0) 
	{
		struct sk_buff *skb2=skb;
		skb=skb_clone(skb, GFP_ATOMIC);		/* Clone the buffer */
		dev_kfree_skb(skb2, FREE_WRITE);
	  	if(skb==NULL)
	  		return 0;
		unlock=0;
	}
	else if(skb->sk) 
	{
		/*
		 *	Packet sent but looped back around. Cease to charge
		 *	the socket for the frame.
		 */
		atomic_sub(skb->truesize, &skb->sk->wmem_alloc);
		skb->sk->write_space(skb->sk);
	}
#endif

	skb->protocol=dosnet_eth_type_trans(skb,dev);
	skb->dev=dev;
	eth=skb->mac.ethernet;

	/* When it is a broadcast packet, we need to 
	   duplicate this packet under one special case - case 6. 
	   And ignore it under case 5. 
	   When dosemu broadcasts, it should be sent to all other dosemu's 
	   AND  linux. 
	   Duplicated packet -> no change in dest address. 
	   Original broadcast packet: change dest. address.
	   */
	if (  (eth->h_dest[0] == 0xff) && (eth->h_dest[1] == 0xff) ) {
		/* broadcast packet. */
		/* Two cases: from dosemu OR from linux(i.e. dsn0). 
		   The dosemu is identified by first three bytes of ethernet 
		   address as follows.
		   */
		if( (eth->h_source[0] == dosnet_generic_address[0]) &&
		    (eth->h_source[1] == dosnet_generic_address[1]) &&
		    (eth->h_source[3] == dosnet_generic_address[3]) ) {
                  /* Broadcast packet by dosemu. Duplicate and send to 
                     linux, and send to other dosemu's.
		     */
			struct sk_buff *new_skb;
			new_skb = skb_clone(skb, GFP_ATOMIC);
			if (new_skb != NULL) {
				/* It is broadcast packet with 
				   source=dosemu, so retain it as is. */
				netif_rx(new_skb);  /* sent to linux side. */
			} else
				stats->tx_dropped++;
		}
		memcpy(eth->h_dest, DOSNET_BROADCAST_ADDRESS, 6 );
	}
	netif_rx(skb);
#if LX_KERNEL_VERSION < 2001000
	if(unlock)
	  	skb_device_unlock(skb);
#endif

#if LX_KERNEL_VERSION >= 2001000
	stats->rx_bytes+=skb->len;
	stats->tx_bytes+=skb->len;
#endif
	stats->rx_packets++;
	stats->tx_packets++;

	return 0;
}

static stats_t * get_stats(struct device *dev)
{
	return (stats_t *)dev->priv;
}

static int dosnet_open(struct device *dev)
{
	int i;
	for(i=0; i<6; i++) {
		dev->dev_addr[i]=DOSNET_DEVICE_ETH_ADDRESS[i];
	}
	MOD_INC_USE_COUNT;
	return 0;
}
static int dosnet_close(struct device *dev)
{
	MOD_DEC_USE_COUNT;
        return 0;
}

/*
 * The driver is promiscious anyway, so don't care about multicasting.
 */

static void dosnet_set_multicast_list( struct device *dev )
{
   /* do nothing */
}

/* Initialize the rest of the DOSNET device. */
int
dosnet_init(struct device *dev)
{
	ether_setup(dev);  

	dev->tbusy		= 0;
	dev->hard_start_xmit	= dosnet_xmit;
	dev->tx_queue_len	= 0;

	dev->open		= dosnet_open;
	dev->stop		= dosnet_close;
	dev->set_multicast_list = dosnet_set_multicast_list;

	dev->priv = kmalloc(sizeof(stats_t), GFP_KERNEL);
	if (dev->priv == NULL)
  		return -ENOMEM;
	memset(dev->priv, 0, sizeof(stats_t));
	dev->get_stats = get_stats;

	return(0);
};




#ifdef MODULE
static char *devicename=DOSNET_DEVICE;
static struct device *dev_dosnet = NULL;

int
init_module(void)
{
	dev_dosnet = (struct device *)kmalloc(sizeof(struct device), GFP_KERNEL);
        memset(dev_dosnet, 0, sizeof(struct device));
        dev_dosnet->name=devicename;
        dev_dosnet->init=&dosnet_init;
        dev_dosnet->next=(struct device *)NULL;


	if (register_netdev(dev_dosnet) != 0)
		return -EIO;
	return 0;
}

void
cleanup_module(void)
{
	if (MOD_IN_USE)
		printk("dosnet: device busy, remove delayed\n");
	else
		unregister_netdev(dev_dosnet);
}
#endif /* MODULE */
