#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char xmlChar;

#define BAD_CAST (xmlChar*)
#define LIBXML_TEST_VERSION

typedef enum {
  XML_ELEMENT_NODE = 1,
  XML_ATTRIBUTE_NODE = 2,
  XML_TEXT_NODE = 3,
  XML_CDATA_SECTION_NODE = 4,
  XML_ENTITY_REF_NODE = 5,
  XML_ENTITY_NODE = 6,
  XML_PI_NODE = 7,
  XML_COMMENT_NODE = 8,
  XML_DOCUMENT_NODE = 9
} xmlElementType;

typedef struct _xmlNs {
  struct _xmlNs* next;
  int type;
  const xmlChar* href;
  const xmlChar* prefix;
} xmlNs, *xmlNsPtr;

typedef struct _xmlAttr {
  void* _private;
  xmlElementType type;
  const xmlChar* name;
  struct _xmlNode* children;
  struct _xmlNode* last;
  struct _xmlNode* parent;
  struct _xmlAttr* next;
  struct _xmlAttr* prev;
  struct _xmlDoc* doc;
  xmlNs* ns;
  int atype;
  void* psvi;
} xmlAttr, *xmlAttrPtr;

typedef struct _xmlNode {
  void* _private;
  xmlElementType type;
  const xmlChar* name;
  struct _xmlNode* children;
  struct _xmlNode* last;
  struct _xmlNode* parent;
  struct _xmlNode* next;
  struct _xmlNode* prev;
  struct _xmlDoc* doc;
  xmlNs* ns;
  xmlChar* content;
  struct _xmlAttr* properties;
  xmlNs* nsDef;
  void* psvi;
  unsigned short line;
  unsigned short extra;
} xmlNode, *xmlNodePtr;

#ifndef xmlChildrenNode
#define xmlChildrenNode children
#endif

typedef struct _xmlDoc {
  void* _private;
  xmlElementType type;
  char* name;
  struct _xmlNode* children;
  struct _xmlNode* last;
  struct _xmlNode* parent;
  struct _xmlNode* next;
  struct _xmlNode* prev;
  struct _xmlDoc* doc;
} xmlDoc, *xmlDocPtr;

xmlDocPtr xmlReadMemory(const char* buffer, int size, const char* URL,
                        const char* encoding, int options);
xmlDocPtr xmlReadFile(const char* filename, const char* encoding, int options);
void xmlFreeDoc(xmlDocPtr cur);
xmlNodePtr xmlDocGetRootElement(const xmlDoc* doc);
void xmlCleanupParser(void);
xmlAttrPtr xmlHasProp(const xmlNode* node, const xmlChar* name);
xmlChar* xmlGetProp(const xmlNode* node, const xmlChar* name);
xmlChar* xmlNodeListGetString(xmlDocPtr doc, const xmlNode* list, int inLine);
void xmlFree(void* mem);

#ifdef __cplusplus
}
#endif
