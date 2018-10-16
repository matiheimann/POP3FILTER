#ifndef MEDIA_TYPES_TREE_H
#define MEDIA_TYPES_TREE_H 

typedef struct treenode* treenodeADT;

typedef struct treenode
{

	treenodeADT* children;
	char* value;
	int wildcard;
	int sons;

}treenode;


typedef struct mediatypetree
{
	
	treenode* root;

}mediatypetree;



mediatypetree* initializeTree();
treenodeADT addMediaTypeToTree(treenode* n, char* mediatype);
void printTree(treenode* n, int size);
int getTypeLength(char* mt);

#endif

