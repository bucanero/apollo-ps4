#include <mxml.h>
#include <string.h>
#include <orbis/RegMgr.h>
#include <orbis/UserService.h>

#include "menu.h"
#include "saves.h"


/*
 * Simple method to parse a XML file, walk down the DOM, 
 * and get the account_id of the xml elements nodes.
 */
int get_xml_owners(const char *xmlfile, int cmd, char*** nam, char*** val)
{
    FILE* fp;
    mxml_node_t *node, *tree = NULL;
    int count = 1;

    /* parse the file and get the DOM */
    if ((fp = fopen(xmlfile, "r")) != NULL)
    {
        tree = mxmlLoadFile(NULL, fp, MXML_NO_CALLBACK);
        fclose(fp);
    }

    if (!tree)
    {
        LOG("XML: could not parse file %s", xmlfile);

        *nam = malloc(sizeof(char*) * 1);
        *val = malloc(sizeof(char*) * 1);
        (*nam)[0] = strdup("Auto-generated Account ID");
        asprintf(&(*val)[0], "%c0", cmd);

        return 1;
    }

    /* Get the user element nodes */
    for (node = mxmlFindElement(tree, tree, "owner", "name", NULL, MXML_DESCEND); node != NULL;
        node = mxmlFindElement(node, tree, "owner", "name", NULL, MXML_DESCEND))
    {
        if (mxmlFindElement(node, node, "user", "account_id", NULL, MXML_DESCEND))
            count++;
    }

    *nam = malloc(sizeof(char*) * count);
    *val = malloc(sizeof(char*) * count);
    (*nam)[0] = strdup("Auto-generated Account ID");
    asprintf(&(*val)[0], "%c0", cmd);
    count = 1;

    /* Get the user element nodes */
    for (node = mxmlFindElement(tree, tree, "owner", "name", NULL, MXML_DESCEND); node != NULL;
        node = mxmlFindElement(node, tree, "owner", "name", NULL, MXML_DESCEND))
    {
        mxml_node_t *value = mxmlFindElement(node, node, "user", "account_id", NULL, MXML_DESCEND);
        
        if (value)
        {
            asprintf(&(*nam)[count], "%s (%s)", mxmlElementGetAttr(node, "name"), mxmlElementGetAttr(value, "account_id"));
            asprintf(&(*val)[count], "%c%s", cmd, mxmlElementGetAttr(value, "account_id"));
            count++;
        }
    }

    for(int i=0; i < count; i++)
        LOG("Owner=%s", (*nam)[i]);

    /* free the document */
    mxmlDelete(tree);

    return count;
}

static const char* xml_whitespace_cb(mxml_node_t *node, int where)
{
    if (where == MXML_WS_AFTER_OPEN || where == MXML_WS_AFTER_CLOSE)
        return ("\n");
    
    return (NULL);
}

int save_xml_owner(const char *xmlfile)
{
    FILE *fp;
    char tmp[0x80];
    mxml_node_t *xml, *node, *group;

    fp = fopen(xmlfile, "w");
    if (!fp)
        return 0;

    // Creates a new document, a node and set it as a root node
    xml = mxmlNewXML("1.0");
    node = mxmlNewElement(xml, "apollo");
    mxmlElementSetAttr(node, "platform", "PS4");
    mxmlElementSetAttr(node, "version", "1.0.0");

    group = mxmlNewElement(node, "owner");
    sceUserServiceGetUserName(apollo_config.user_id, tmp, sizeof(tmp));
    mxmlElementSetAttr(group, "name", tmp);

    node = mxmlNewElement(group, "console");
    mxmlElementSetAttr(node, "idps", "");
    snprintf(tmp, sizeof(tmp), "%016lX %016lX", apollo_config.psid[0], apollo_config.psid[1]);
    mxmlElementSetAttr(node, "psid", tmp);

    node = mxmlNewElement(group, "user");
    snprintf(tmp, sizeof(tmp), "%08x", apollo_config.user_id);
    mxmlElementSetAttr(node, "id", tmp);

    snprintf(tmp, sizeof(tmp), "%016lx", apollo_config.account_id);
    mxmlElementSetAttr(node, "account_id", tmp);

    // Dumping document to file
    LOG("Saving user '%08x' to %s...", apollo_config.user_id, xmlfile);
    mxmlSaveFile(xml, fp, &xml_whitespace_cb);
    fclose(fp);

    // free the document
    mxmlDelete(xml);

    return 1;
}
