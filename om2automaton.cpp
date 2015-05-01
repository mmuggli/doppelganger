// om2automaton <binary optical map> <gcsa format graph file>



#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <vector>
#include <map>
#include <iostream>

class Node {
public:
    Node(unsigned int _label, unsigned int _value, unsigned int _pos) { 

        label = _label; 
        value = _value; 
        
        if (_label == 0) {
            int ijk = 0;
        }



        pos = _pos;}
    unsigned int label;
    unsigned int value;
    unsigned int pos;
};

class Edge {
public:
    Edge(Node *_from, Node *_to) { from = _from; to = _to;}
    Node *from;
    Node *to;
};


unsigned int remap(unsigned int l)
{
    return l;
    if (l == 0) return 0;
    if (l == (unsigned int)-1) return 255;
    //FIXME: add log stuff in here
    float max = 1364039;//(unsigned int)-1;
    float thelog = log(l);
    float ret =  thelog * 253 / log(max)  + 1;

    return ret;

}

unsigned int mytoupper(unsigned int lab)
{
        if (lab == 0) return lab;
        if (lab & 0x1) lab -= 1; // mark it as a backbone "uppercase" node
        return lab;
}

unsigned int mytolower(unsigned int lab)
{
        lab |= 0x1; // mark it as a nonbackbone "lowercase" node        
        return lab;
}
const int BIN_SIZE = 100;
unsigned int quantize(unsigned int val)
{
    unsigned int new_val = 0;
    if (val % BIN_SIZE < BIN_SIZE / 2.0)
        new_val = val - val % BIN_SIZE;
    else
        new_val = val - val % BIN_SIZE + BIN_SIZE;
    return new_val;
}


int main(int argc, char** argv)
{
    if (argc < 3) {
        printf("Usage: %s <binary optical map> <gcsa format graph>\n", argv[0]);
        exit(1);
    }

    std::map<unsigned int, unsigned int> counts;
    unsigned int elems = 364876384 / 4;
    int ifd = 0;
    char *addr = 0;
    char *ifname = argv[1];
    ifd = open(ifname, O_RDONLY);
//printf("Attempting to mmap %d elements from %s\n", (elems - 1) * 4, fname);
    // // void *mmap(void *addr, size_t length, int prot, int flags,
    // //            int ifd, off_t offset);
    addr = (char*)malloc(elems * 4);
    off_t  pa_offset = 0;
    
    printf("Reading file %s\n", ifname);
    int r = read(ifd, addr, elems * 4);
    printf("read %d bytes\n", r);
    if (r != 4*elems) { printf("Incomplete read ERROR!\n");}

    int j = 0;
    Node *start = new Node(-2, 0, 0);
    ++j;
    Node *current = start;
    std::vector<Node *> nodes;
    std::vector<Edge *> edges;
    nodes.push_back(start);

    printf("building baseline\n");
    int i;

    for (i = 0; i < r / 4; ++i) {

        unsigned int lab = mytoupper(quantize(((unsigned int *)addr)[i]));

        Node *n = new Node(lab, j, j);
        ++j;
        nodes.push_back(n);
        Edge *e = new Edge(current, n);
        edges.push_back(e);
        current = n;
    }


    Node *end = new Node(0, j, j);
    j++;
    nodes.push_back(end);
    std::cout << "creating edge from " << current->pos << " to " << end->pos << std::endl;
    Edge *e = new Edge(current, end);
    edges.push_back(e);

    std::vector<Node *> skip_nodes;
    std::vector<Edge *> skip_edges;


    std::cout << "Adding skip edges" << std::endl;;
    for (i = 0; i < r / 4 - 1; ++i) {
        // add order 1 skip nodes
        unsigned int lab = mytolower(nodes[i+1]->label + nodes[i+2]->label);

        Node *sumnode = new Node(lab, nodes[i+1]->value, j);
        ++j;
        nodes.push_back(sumnode);
        Edge *e1 = new Edge(nodes[i], sumnode);
        edges.push_back(e1);
        Edge *e2 = new Edge(sumnode, nodes[i+3]);
        edges.push_back(e2);


        //add order 2 skip nodes
        if (i  < r / 4 - 4) {
            unsigned int lab = mytolower(nodes[i+1]->label + nodes[i+2]->label + nodes[i+3]->label);

            Node *sumnode = new Node(lab, nodes[i+1]->value, j);
            ++j;
            nodes.push_back(sumnode);
            Edge *e1 = new Edge(nodes[i], sumnode);
            edges.push_back(e1);
            Edge *e2 = new Edge(sumnode, nodes[i+4]);
            edges.push_back(e2);
        }

    }
    char *ofname = argv[2];
    printf("writing file %s\n", ofname);
    int ofd = open(ofname, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (ofd == -1) printf("problem opening file to write ERROR!\n");
    unsigned int numnodes = nodes.size();
    unsigned int numedges = edges.size();
    write(ofd, &numnodes, 4);
    write(ofd, &numedges, 4);
    for (std::vector<Node *>::iterator ni = nodes.begin(); ni != nodes.end(); ++ni) {
        unsigned int lab = ((*ni)->label);
        if (lab == 0) std::cout << "0 lab at node " << (*ni)->value <<  " pos " << (*ni)->pos << std::endl;
        lab = remap(lab);
        
        counts[lab] += 1;
        write(ofd, &lab, 4);
        write(ofd, &((*ni)->value), 4);
    }

    std::cout << "Number of nodes: " << numnodes << std::endl;
    std::cout << "largest node pos: " << (*(nodes.end() - 1))->pos << std::endl;
    for (std::vector<Edge *>::iterator ei = edges.begin(); ei != edges.end(); ++ei) {
        write(ofd, &((*ei)->from->pos), 4);
        write(ofd, &((*ei)->to->pos), 4);
    }

    printf("size of active alphabet: %d\n", counts.size());

    // for(std::map<unsigned int, unsigned int>::iterator ci = counts.begin(); ci != counts.end(); ++ci) {
    //     std::cout << "counts[" << ci->first << "] = " << ci->second << std::endl;
    // }

    close(ofd);
}
