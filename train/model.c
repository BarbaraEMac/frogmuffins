/*
 * Track Model
 * becmacdo
 * dgoc
 * Copied and modified from course webpage
 */

#define DEBUG 	1
#define INT_MAX	0x7FFFFFFF

#include <debug.h>
#include <string.h>
#include <math.h>

#include "model.h"
#include "servers.h"
#include "requests.h"

int parse_edges_str(FILE *file, TrackModel *model, Node *node) {

	// See how many edges there are
	int num_edges;
	if (fscanf(file, " %d", &num_edges) < 1) {
		eprintf("Could not get number of edges\r\n");
		return -1;
	}
	
	// Parse the edges
	int j, dest;
	Edge *edge = 0;

	for (j = 0; j < num_edges; j++) {
		if (fscanf(file, " %d", &dest) < 1) {
			eprintf("Could not get edge destination\r\n");
			return -1;
		}

		if (dest < 0 || dest > model->num_nodes) {
			eprintf("edge destination %d out of bounds\r\n", dest);
			return -1;
		}
		// See of the destination node has already been allocated
		if (model->nodes[dest].type != 0 ) {
			Edge **other;
			foreach (other, model->nodes[dest].edges) {
				if( *other != 0 && (*other)->node2 == node ) {
					edge = *other;
				}
			}
			assert(edge->node1 == &model->nodes[dest]);
		} else { 
			// Grab a brand new edge
			edge = &model->edges[model->num_edges++];
			edge->node1 = node;
			edge->node2 = &model->nodes[dest];
			assert( model->num_edges < MAX_NUM_EDGES );
		}
		
		char edgetype[10];
		memoryset(edgetype, 0, 10);
		if (fscanf(file, " %9s", edgetype) < 1) {
			eprintf("Could not get edge type\r\n");
			return -1;
		}

		if (fscanf(file, " %d", &edge->distance) < 1) {
			eprintf("Could not get edge distance\r\n");
			return -1;
		}
		
		switch (node->type) {
			case NODE_SENSOR:
				if (strcmp(edgetype, "ahead") == 0) {
					node->se.ahead = edge;
				} else if (strcmp(edgetype, "behind") == 0) {
					node->se.behind = edge;
				} else {
					eprintf("Invalid edge direction %s\r\n", edgetype);
					return -1;
				}
				break;
			case NODE_SWITCH:
				if (strcmp(edgetype, "behind") == 0) {
					node->sw.behind = edge;
				} else if (strcmp(edgetype, "curved") == 0) {
					node->sw.ahead[SWITCH_CURVED] = edge;
				} else if (strcmp(edgetype, "straight") == 0) {
					node->sw.ahead[SWITCH_STRAIGHT] = edge;
				} else {
					eprintf("Invalid edge direction %s\n", edgetype);
					return -1;
				}
				break;

			case NODE_STOP:
				if (strcmp(edgetype, "ahead") == 0) {
					node->st.ahead = edge;
				} else {
					eprintf("Invalid edge direction %s\n", edgetype);
					return -1;
				}

				break;
		}
	}
	return 0;
}

int parse_model_str(FILE *file, TrackModel* model)
{
	char init[10];
	memoryset(init, 0, 10);
	memoryset(model, 0, sizeof(TrackModel));
	model->num_edges = 0;

	if (fscanf(file, " %9s", init) < 1) {
		eprintf("Could not read initial string.\r\n");
		return -1;
	}

	if (strcmp(init, "track") != 0) {
		eprintf("Expected 'track', got '%s'\r\n", init);
		return -1;
	}

	model->num_nodes = -1;
	
	if (fscanf(file, " %d", &model->num_nodes) < 1) {
		eprintf("Did not get num_nodes\r\n");
		return -1;
	}

	if (model->num_nodes < 0) {
		eprintf("num_nodes must be positive\r\n");
		return -1;
	}
	//model->nodes = malloc(sizeof(Node)*model->num_nodes);

	if (model->num_nodes > MAX_NUM_NODES) {
		eprintf("Could not allocate space for %d nodes.\r\n", model->num_nodes);// Ha!
		return -1; 
	}
	
	int i;
	for (i = 0; i < model->num_nodes; i++) {

	debug("parsing node %d\r\n", i );
		int num;
		if (fscanf(file, " %d", &num) < 1) {
			eprintf("Could not get index\r\n");
			return -1;
		}
		if (num < 0 || num >= model->num_nodes) {
			eprintf("%d is an invalid index\r\n", num);
			return -1;
		}

		Node* node = &model->nodes[num];
		// Store the index
		node->idx 	   = num;
		node->reserved = 0;
		node->reserver = -1;

		char name[5];
		memoryset(name, 0, 5);
		if (fscanf(file, " %4s", name) < 1) {
			eprintf("Could not get name\r\n");
			return -1;
		}

		strncpy(node->name, name, sizeof(node->name));

		char type[10];
		memoryset(type, 0, 10);
		if (fscanf(file, " %9s", type) < 1) {
			eprintf("Could not get node type\r\n");			
			return -1;
		}

		if (fscanf(file, " %d", &node->id) < 1) {
			eprintf("Could not get id\r\n");
			return -1;
		}

		if (fscanf(file, " %d", &node->x) < 1) {
			eprintf("Could not get x coordinate\r\n");
			return -1;
		}
		if (fscanf(file, " %d", &node->y) < 1) {
			eprintf("Could not get y coordinate\r\n");
			return -1;
		}

		if (strcmp(type, "sensor") == 0) {
			// Parse a sensor
			node->type = NODE_SENSOR;
			
			if (parse_edges_str(file, model, node) < 0) return -1;

		} else if (strcmp(type, "switch") == 0) {
			// Parse a switch
			node->type = NODE_SWITCH;
			
			char defaultdir[10];
			memoryset(defaultdir, 0, 10);
			if (fscanf(file, " %9s", defaultdir) < 1) return -1;
			if (strcmp(defaultdir, "straight") == 0) {
				node->sw.set = SWITCH_STRAIGHT;
			} else if (strcmp(defaultdir, "curved") == 0) {
				node->sw.set = SWITCH_CURVED;
			} else {
				return -1;
			}
			
			if (parse_edges_str(file, model, node) < 0) return -1;

		} else if (strcmp(type, "stop") == 0) {
			// Parse a sensor
			node->type = NODE_STOP;
			
			if (parse_edges_str(file, model, node) < 0) return -1;

		} else {
			return -1;
		}
	}

	 // Whew, all parsed.
	return 0;
}

int parse_model(int trackId, TrackModel* model)
{
// A
char * track[] = {
" track 72 \
0 A1 sensor 0 733 67 2 \
	51 ahead 218 \
	66 behind 520 \
1 A3 sensor 1 740 350 2 \
	53 ahead 62 \
	15 behind 440 \
2 A5 sensor 2 915 1170 2 \
	42 ahead 216 \
	12 behind 665 \
3 A7 sensor 3 753 1093 2 \
	13 ahead 473 \
	41 behind 211 \
4 A9 sensor 4 565 1020 2 \
	11 ahead 289 \
	40 behind 210 \
5 A11 sensor 5 305 940 2 \
	40 ahead 500 \
	71 behind 50 \
6 A13 sensor 6 565 147 2 \
	43 ahead 215 \
	65 behind 320 \
7 A15 sensor 7 400 215 2 \
	70 ahead 150 \
	43 behind 400 \
8 B1 sensor 8 1380 1020 2 \
	30 ahead 398 \
	55 behind 225 \
9 B3 sensor 9 1380 953 2 \
	16 ahead 205 \
	55 behind 220 \
10 B5 sensor 10 1380 147 2 \
	25 ahead 405 \
	52 behind 214 \
11 B7 sensor 11 290 1020 2 \
	4 ahead 289 \
	67 behind 50 \
12 B9 sensor 12 250 1170 2 \
	2 ahead 665 \
	68 behind 55 \
13 B11 sensor 13 270 1093 2 \
	3 ahead 473 \
	69 behind 55 \
14 B13 sensor 14 1660 810 2 \
	58 ahead 224 \
	31 behind 205 \
15 B15 sensor 15 740 786 2 \
	1 ahead 440 \
	54 behind 70 \
16 C1 sensor 16 1530 810 2 \
	9 ahead 205 \
	61 behind 208 \
17 C3 sensor 17 1895 1170 2 \
	64 ahead 410 \
	44 behind 226 \
18 C5 sensor 18 1090 1093 2 \
	45 ahead 79 \
	54 behind 411 \
19 C7 sensor 19 1275 1170 2 \
	57 ahead 215 \
	42 behind 145 \
20 C9 sensor 20 1020 1020 2 \
	54 ahead 305 \
	55 behind 146 \
21 C11 sensor 21 1035 147 2 \
	52 ahead 140 \
	53 behind 314 \
22 C13 sensor 22 1210 67 2 \
	35 ahead 875 \
	50 behind 60 \
23 C15 sensor 23 1380 1093 2 \
	29 ahead 405 \
	45 behind 223 \
24 D1 sensor 24 1660 350 2 \
	59 ahead 207 \
	33 behind 205 \
25 D3 sensor 25 1785 147 2 \
	49 ahead 223 \
	10 behind 405 \
26 D5 sensor 26 2405 290 2 \
	34 ahead 375 \
	48 behind 213 \
27 D7 sensor 27 2405 210 2 \
	48 ahead 290 \
	35 behind 375 \
28 D9 sensor 28 2405 962 2 \
	37 ahead 375 \
	47 behind 291 \
29 D11 sensor 29 1785 1093 2 \
	23 ahead 405 \
	46 behind 220 \
30 D13 sensor 30 1785 1020 2 \
	8 ahead 398 \
	56 behind 225 \
31 D15 sensor 31 1785 953 2 \
	14 ahead 205 \
	56 behind 230 \
32 E1 sensor 32 1530 350 2 \
	60 ahead 222 \
	39 behind 205 \
33 E3 sensor 33 1785 210 2 \
	24 ahead 205 \
	49 behind 220 \
34 E5 sensor 34 2075 147 2 \
	26 ahead 375 \
	49 behind 74 \
35 E7 sensor 35 2075 67 2 \
	27 ahead 375 \
	22 behind 875 \
36 E9 sensor 36 2405 885 2 \
	47 ahead 215 \
	38 behind 375 \
37 E11 sensor 37 2075 1093 2 \
	28 ahead 375 \
	46 behind 65 \
38 E13 sensor 38 2075 1020 2 \
	56 ahead 62 \
	36 behind 375 \
39 E15 sensor 39 1380 210 2 \
	52 ahead 230 \
	32 behind 205 \
40 SW1 switch 1 775 1045 curved 3 \
	5 straight 500 \
	4 curved 210 \
	41 behind 191 \
41 SW2 switch 2 950 1125 curved 3 \
	40 straight 191 \
	3 curved 211 \
	42 behind 182 \
42 SW3 switch 3 1130 1170 curved 3 \
	2 straight 216 \
	41 curved 182 \
	19 behind 145 \
43 SW4 switch 4 775 110 straight 3 \
	7 straight 400 \
	6 curved 215 \
	51 behind 185 \
44 SW5 switch 5 1680 1170 curved 3 \
	17 straight 226 \
	46 curved 334 \
	57 behind 187 \
45 SW6 switch 6 1170 1093 straight 3 \
	23 straight 223 \
	57 curved 338 \
	18 behind 79 \
46 SW7 switch 7 2010 1093 straight 3 \
	29 straight 220 \
	44 curved 334 \
	37 behind 65 \
47 SW8 switch 8 2480 680 curved 3 \
	28 straight 291 \
	36 curved 215 \
	48 behind 193 \
48 SW9 switch 9 2480 495 curved 3 \
	27 straight 290 \
	26 curved 213 \
	47 behind 193 \
49 SW10 switch 10 2010 147 straight 3 \
	25 straight 223 \
	33 curved 220 \
	34 behind 74 \
50 SW11 switch 11 1140 67 curved 3 \
	51 straight 190 \
	53 curved 445 \
	22 behind 60 \
51 SW12 switch 12 950 67 curved 3 \
	0 straight 218 \
	43 curved 185 \
	50 behind 190 \
52 SW13 switch 13 1170 147 straight 3 \
	10 straight 214 \
	39 curved 230 \
	21 behind 140 \
53 SW14 switch 14 775 305 curved 3 \
	50 straight 445 \
	21 curved 314 \
	1 behind 62 \
54 SW15 switch 15 775 845 curved 3 \
	18 straight 411 \
	20 curved 305 \
	15 behind 70 \
55 SW16 switch 16 1170 1020 straight 3 \
	8 straight 225 \
	9 curved 220 \
	20 behind 146 \
56 SW17 switch 17 2010 1020 straight 3 \
	30 straight 225 \
	31 curved 230 \
	38 behind 62 \
57 SW18 switch 18 1490 1170 curved 3 \
	19 straight 215 \
	45 curved 338 \
	44 behind 187 \
61 SW99 switch 0x99 1595 620 curved 3 \
	62 straight 230 \
	16 curved 208 \
	58 behind 24 \
58 SW9A switch 0x9a 1595 600 straight 3 \
	61 straight 24 \
	14 curved 224 \
	60 behind 32 \
59 SW9B switch 0x9b 1595 540 straight 3 \
	63 straight 247 \
	24 curved 207 \
	60 behind 24 \
60 SW9C switch 0x9c 1595 560 curved 3 \
	59 straight 24 \
	32 curved 222 \
	58 behind 32 \
62 DE1 stop 0 1595 850 1 \
	61 ahead 230 \
63 DE2 stop 1 1595 290 1 \
	59 ahead 247 \
64 DE3 stop 2 2310 1170 1 \
	17 ahead 410 \
65 DE4 stop 3 230 147 1 \
	6 ahead 320 \
66 DE5 stop 4 210 67 1 \
	0 ahead 520 \
67 DE7 stop 6 240 1020 1 \
	11 ahead 50 \
68 DE9 stop 8 195 1170 1 \
	12 ahead 55 \
69 DE10 stop 9 215 1093 1 \
	13 ahead 55 \
70 DE6 stop 5 250 215 1 \
	7 ahead 150 \
71 DE8 stop 7 255 940 1 \
	5 ahead 50",
// B
" track 70 \
0 A1 sensor 0 733 67 2 \
	51 ahead 218 \
	66 behind 520 \
1 A3 sensor 1 740 350 2 \
	53 ahead 62 \
	15 behind 440 \
2 A5 sensor 2 915 1170 2 \
	42 ahead 216 \
	12 behind 665 \
3 A7 sensor 3 753 1093 2 \
	13 ahead 473 \
	41 behind 211 \
4 A9 sensor 4 565 1020 2 \
	11 ahead 289 \
	40 behind 210 \
5 A11 sensor 5 545 925 2 \
	40 ahead 265 \
	7 behind 774 \
6 A13 sensor 6 565 147 2 \
	43 ahead 215 \
	65 behind 320 \
7 A15 sensor 7 545 230 2 \
	5 ahead 774 \
	43 behind 260 \
8 B1 sensor 8 1380 1020 2 \
	30 ahead 398 \
	55 behind 225 \
9 B3 sensor 9 1380 953 2 \
	16 ahead 205 \
	55 behind 220 \
10 B5 sensor 10 1380 147 2 \
	25 ahead 405 \
	52 behind 214 \
11 B7 sensor 11 290 1020 2 \
	4 ahead 289 \
	67 behind 55 \
12 B9 sensor 12 250 1170 2 \
	2 ahead 665 \
	68 behind 50 \
13 B11 sensor 13 270 1093 2 \
	3 ahead 473 \
	69 behind 55 \
14 B13 sensor 14 1660 810 2 \
	58 ahead 224 \
	31 behind 205 \
15 B15 sensor 15 740 786 2 \
	1 ahead 440 \
	54 behind 70 \
16 C1 sensor 16 1530 810 2 \
	9 ahead 205 \
	61 behind 208 \
17 C3 sensor 17 1895 1170 2 \
	64 ahead 410 \
	44 behind 226 \
18 C5 sensor 18 1090 1093 2 \
	45 ahead 79 \
	54 behind 411 \
19 C7 sensor 19 1275 1170 2 \
	57 ahead 215 \
	42 behind 145 \
20 C9 sensor 20 1020 1020 2 \
	54 ahead 305 \
	55 behind 146 \
21 C11 sensor 21 1035 147 2 \
	52 ahead 140 \
	53 behind 314 \
22 C13 sensor 22 1210 67 2 \
	35 ahead 785 \
	50 behind 60 \
23 C15 sensor 23 1380 1093 2 \
	29 ahead 405 \
	45 behind 223 \
24 D1 sensor 24 1660 350 2 \
	59 ahead 207 \
	33 behind 205 \
25 D3 sensor 25 1785 147 2 \
	49 ahead 223 \
	10 behind 405 \
26 D5 sensor 26 2310 290 2 \
	34 ahead 275 \
	48 behind 213 \
27 D7 sensor 27 2310 210 2 \
	48 ahead 290 \
	35 behind 375 \
28 D9 sensor 28 2310 962 2 \
	37 ahead 284 \
	47 behind 291 \
29 D11 sensor 29 1785 1093 2 \
	23 ahead 405 \
	46 behind 220 \
30 D13 sensor 30 1785 1020 2 \
	8 ahead 398 \
	56 behind 225 \
31 D15 sensor 31 1785 953 2 \
	14 ahead 205 \
	56 behind 230 \
32 E1 sensor 32 1530 350 2 \
	60 ahead 222 \
	39 behind 205 \
33 E3 sensor 33 1785 210 2 \
	24 ahead 205 \
	49 behind 220 \
34 E5 sensor 34 2080 147 2 \
	26 ahead 275 \
	49 behind 74 \
35 E7 sensor 35 1985 67 2 \
	27 ahead 375 \
	22 behind 785 \
36 E9 sensor 36 2310 885 2 \
	47 ahead 215 \
	38 behind 275 \
37 E11 sensor 37 2065 1093 2 \
	28 ahead 284 \
	46 behind 65 \
38 E13 sensor 38 2065 1020 2 \
	56 ahead 62 \
	36 behind 275 \
39 E15 sensor 39 1380 210 2 \
	52 ahead 230 \
	32 behind 205 \
40 SW1 switch 1 775 1045 curved 3 \
	5 straight 245 \
	4 curved 210 \
	41 behind 191 \
41 SW2 switch 2 950 1125 curved 3 \
	40 straight 191 \
	3 curved 211 \
	42 behind 182 \
42 SW3 switch 3 1130 1170 curved 3 \
	2 straight 216 \
	41 curved 182 \
	19 behind 145 \
43 SW4 switch 4 775 110 straight 3 \
	7 straight 260 \
	6 curved 215 \
	51 behind 185 \
44 SW5 switch 5 1680 1170 curved 3 \
	17 straight 226 \
	46 curved 334 \
	57 behind 187 \
45 SW6 switch 6 1170 1093 straight 3 \
	23 straight 223 \
	57 curved 338 \
	18 behind 79 \
46 SW7 switch 7 2010 1093 straight 3 \
	29 straight 220 \
	44 curved 334 \
	37 behind 65 \
47 SW8 switch 8 2385 680 curved 3 \
	28 straight 291 \
	36 curved 215 \
	48 behind 193 \
48 SW9 switch 9 2385 495 curved 3 \
	27 straight 290 \
	26 curved 213 \
	47 behind 193 \
49 SW10 switch 10 2010 147 straight 3 \
	25 straight 223 \
	33 curved 220 \
	34 behind 74 \
50 SW11 switch 11 1140 67 curved 3 \
	51 straight 190 \
	53 curved 445 \
	22 behind 60 \
51 SW12 switch 12 950 67 curved 3 \
	0 straight 218 \
	43 curved 185 \
	50 behind 190 \
52 SW13 switch 13 1170 147 straight 3 \
	10 straight 214 \
	39 curved 230 \
	21 behind 140 \
53 SW14 switch 14 775 305 curved 3 \
	50 straight 445 \
	21 curved 314 \
	1 behind 62 \
54 SW15 switch 15 775 845 curved 3 \
	18 straight 411 \
	20 curved 305 \
	15 behind 70 \
55 SW16 switch 16 1170 1020 straight 3 \
	8 straight 225 \
	9 curved 220 \
	20 behind 146 \
56 SW17 switch 17 2010 1020 straight 3 \
	30 straight 225 \
	31 curved 230 \
	38 behind 62 \
57 SW18 switch 18 1490 1170 curved 3 \
	19 straight 215 \
	45 curved 338 \
	44 behind 187 \
61 SW99 switch 0x99 1595 620 curved 3 \
	62 straight 230 \
	16 curved 208 \
	58 behind 24 \
58 SW9A switch 0x9a 1595 600 straight 3 \
	61 straight 24 \
	14 curved 224 \
	60 behind 32 \
59 SW9B switch 0x9b 1595 540 straight 3 \
	63 straight 220 \
	24 curved 207 \
	60 behind 24 \
60 SW9C switch 0x9c 1595 560 curved 3 \
	59 straight 24 \
	32 curved 222 \
	58 behind 32 \
62 DE1 stop 0 1595 850 1 \
	61 ahead 230 \
63 DE2 stop 1 1595 320 1 \
	59 ahead 220 \
64 DE3 stop 2 2310 1170 1 \
	17 ahead 410 \
65 DE4 stop 3 230 147 1 \
	6 ahead 320 \
66 DE5 stop 4 210 67 1 \
	0 ahead 520 \
67 DE7 stop 6 235 1020 1 \
	11 ahead 55 \
68 DE9 stop 8 200 1170 1 \
	12 ahead 50 \
69 DE10 stop 9 215 1093 1 \
	13 ahead 55"
};

	FILE f;

	if( trackId < 0 || trackId > 1 ) {
		eprintf("Invalid track id %d.\r\n", trackId);
	}
	f.curr = track[trackId];
	int ret = parse_model_str(&f, model);
	/*int i, x, y, hyp;
	Edge *e;
	for( i = 0; i < model->num_edges; i++ ) {
		e = &model->edges[i];
		x = e->node1->x - e->node2->x;
		y = e->node1->y - e->node2->y;
		hyp = isqrt( x * x + y * y );
		if( abs(e->distance - hyp) >= 20 )
		printf("Edge %s-%s \tdist: %d, \thyp: %d, diff: %d\r\n",
				e->node1->name, e->node2->name, e->distance,
				hyp, e->distance - hyp );
	}*/
	return ret;
}
/*
void model_findNextNodes( TrackModel *model, Node *curr, Node *prev,
							Node *next1, Node *next2 ) {
	Node *n1 = 0;	// Tmp neighbour nodes
	Node *n2 = 0;
	Node *n3 = 0;
	
	switch ( curr->type ) {
		case NODE_SWITCH:
			n1 = (curr->sw.ahead[0]->node1 == curr) ? 
				curr->sw.ahead[0].node2 : curr->sw.ahead[0]->node1;
			n2 = curr->sw.ahead[1]->dest;
			n3 = curr->sw.behind->dest;
			
			//TODO: DEPENDS ON THE ROUTE
			if ( (n1 == prev) || (n2 == prev) ) {
				next1 = n3;
				next2 = 0;
			} else { 
				next1 = n1;
				next2 = n2;
			}
			
			break;`
		
		case NODE_SENSOR:
			n1 = &model->nodes[curr->se.ahead.dest];
			n2 = &model->nodes[curr->se.behind.dest];
			
			next1 = ( n1 == prev ) ? n2 : n1;
			next2 = 0;

			break;
		
		case NODE_STOP:
			n1 = &model->nodes[curr->st.ahead.dest];

			// You are heading to a stop
			next1 = (n1 == prev) ? 0 : n1;
			next2 = 0;
			
			break;
	}
}*/

Node *
node_neighbour( Node *node, Edge *edge ) {
	if( edge->node1 == node ) {
		return edge->node2;
	} else {
		assert( edge->node2 == node );
		return edge->node1; 
	}
}

int model_nameToIdx ( TrackModel *model, const char *name ) {
	int	 n = model->num_nodes;
	int	 i;
	int	 num;
	char	copy[NODE_NAME_LEN];
	const char *ch = &copy[1];

	// make a local copy so we cah change it if need be
	strncpy( copy, name, NODE_NAME_LEN );

	// name too long
	if( copy[NODE_NAME_LEN - 1] != '\0' ) return NOT_FOUND;

	// Special case for even numbered sensors
	num = atoi( &ch );
	if ( (copy[0] >= 'A' && copy[0] <= 'E') && 
		 (copy[1] >= '1' && copy[1] <= '9') && 
		 (num % 2 == 0) ) {
		num -= 1;
		itoa( num, &copy[1] );
	}

	for ( i = 0; i < n; i ++ ) {
		if ( strcmp(model->nodes[i].name, copy) == 0 ) {
			return i;
		}
	}
	return NOT_FOUND;
}

// format sensor id into human readable output
char sensor_bank( int sensor ) {
	return 'A' + (sensor / SIZE_BANK);
}
char sensor_num( int sensor ) {
	return (sensor % SIZE_BANK) + 1;
}

char switch_dir( SwitchDir dir ) {
	return (dir == SWITCH_CURVED) ? 'C' : 'S'; 
}

// check if the direction index is within range
SwitchDir switch_init( char dir ) {
		if( dir == 's' || dir == 'S' ) { return SWITCH_STRAIGHT; }
		if( dir == 'c' || dir == 'C' ) { return SWITCH_CURVED; }
		
		return INVALID_DIR;
}

int node_dist( Node *n1, Node *n2 ) {
	int i;

	for ( i = 0; i < n1->type; i ++ ) {
		if ( node_neighbour( n1, n1->edges[i] ) == n2 ) {
			return n1->edges[i]->distance;
		}
	}

	return INT_MAX;
}

// Convert a sensor index into a node index
inline int sIdxToIdx ( int sIdx ) {
	return (int) (sIdx / 2);
}

inline int idxTosIdx (int idx, char *name) {
	int ret = idx * 2;
	if ( (atod(name[1]) % 2) == 0 ) {
		ret += 1;
	}
	debug ("idx=%d ret=%d\r\n", idx, ret);
	return ret;
}

/*
char * model_idxToName ( TrackModel *model, int idx ) {
	return model->nodes[idx].name;
}

*/
