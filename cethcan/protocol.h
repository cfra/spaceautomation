			/* addrs: SSSEEEEE
			 *           8 -> extended addr
			 *          14 -> unused
			 */
#define CANA_TIME		0x10080000
#define CANA_DISCOVERY		0x4c080000
#define CANA_DISCOVERY_F(pg,dst) (CANA_DISCOVERY | (((uint32_t)(pg) & 0xf) << 12) | ((dst) & 0xfff))
#define CANA_LIGHT		0xcc080000
#define CANA_LIGHT_F(src,dst)	(CANA_LIGHT | (((src) & 0x3f) << 12) | ((dst) & 0xfff))
#define CANA_SENSOR		0xe6080000
#define CANA_SENSOR_F(src)	(CANA_SENSOR | ((src) & 0xfff))
#define CANA_DEBUG		0xe7000000

#define CANA_PROTOCOL		0xffe80000

/*
#define can_rx_isext()		(can_rx_addr.b[1] & 0x08)
#define can_rx_ext_rr()		(can_rx_dlc & 0x40)
#define can_rx_len()		(can_rx_dlc & 0x0f)

#define can_rx_sublab_proto()	((can_rx_addr.b[0] << 8) | (can_rx_addr.b[1] & 0xe8))
#define can_rx_sublab_addr()	(((can_rx_addr.b[2] & 0x0f) << 8) | can_rx_addr.b[3])
#define can_rx_sublab_disco_page()	((can_rx_addr.b[2] & 0xf0) >> 4)
*/
