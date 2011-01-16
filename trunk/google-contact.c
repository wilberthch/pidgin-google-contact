#define PURPLE_PLUGINS

#include <plugin.h>
#include <version.h>

#include <util.h>
#include <blist.h>
#include <xmlnode.h>
#include <account.h>
#include <prefs.h>
#include <pluginpref.h>
#include <signal.h>

#include <string.h>
#include <time.h>
#include <glib.h>

#define USER_AGENT "pidgin-google-contact"

typedef enum {GTALK, AIM, YAHOO, MSN, QQ, JABBER, ICQ, SKYPE,
              POTHER, GOTHER} AccountType;

/* function prototypes */
static AccountType ptype(PurpleAccount *pAccount);
static AccountType gtype(xmlnode *gIM);
static void sign_in_cb(PurpleAccount *pAccount, gpointer data);
static void run(PurpleAccount *pAccount);
static void purplemerger(xmlnode *glist);
static void logincb(PurpleUtilFetchUrlData *url_data, gpointer user_data, 
        const gchar *url_text, gsize len, const gchar *error_message);
static void getlistcb(PurpleUtilFetchUrlData *url_data, gpointer user_data,
        const gchar *url_text, gsize len, const gchar *error_message);
static xmlnode * findgooglecontact(PurpleBuddy *buddy, xmlnode *glist);
static gboolean updateglist(xmlnode *gcontact, PurpleContact *pcontact);
static void upload(xmlnode *gcontact, char *auth, int len);
static void uploadcb(PurpleUtilFetchUrlData *url_data, gpointer user_data,
        const gchar *url_text, gsize len, const gchar *error_message);
static xmlnode * getedituri(xmlnode *gcontact);

/* function definitions */
static void uploadcb(PurpleUtilFetchUrlData *url_data, gpointer user_data,
        const gchar *url_text, gsize len, const gchar *error_message) {
    if (error_message != NULL)
        /* if an error occured in upload, the plugin was not successful */
        purple_prefs_set_bool("/plugins/core/google-contact/success",
                FALSE);
    return;
}

static void sign_in_cb(PurpleAccount *pAccount, gpointer data) {
    /* if the account that signed in was a google talk account */
    if (ptype(pAccount) == GTALK) {
        /* run the plugin */
        run(pAccount);
    }
}

static void run(PurpleAccount *pAccount) {
    gchar **split;
    gchar *url, *request, *logindata, *username;
    const char *host = "www.google.com";
    /* copy relevant part of username before the @ */
    split = g_strsplit(pAccount->username, "@", 2);
    username = g_strdup(split[0]);
    /* login url request */
    url = g_strdup_printf("https://%s/accounts/ClientLogin", host);
    logindata = g_strdup_printf(
            "accountType=GOOGLE&Email=%s%%40gmail.com"
            "&Passwd=%s&service=cp&source=%s",
            username, pAccount->password, USER_AGENT);
    request = g_strdup_printf("POST %s HTTP/1.0\r\n"
            "Host: %s\r\n"
            "Content-Length: %d\r\n"
            "Content-Type: application/x-www-form-urlencoded\r\n\r\n"
            "%s", url, host, strlen(logindata), logindata);
    purple_util_fetch_url_request(url, TRUE, USER_AGENT, FALSE,
            request, TRUE, (PurpleUtilFetchUrlCallback) logincb, 
            NULL);
    /* free memory */
    g_free(url);
    g_free(request);
    g_free(logindata);
    g_free(username);
    g_strfreev(split);

    /* by this point, the plugin has synchronized with this 
     * google talk pAccount
     * returning TRUE here means that the plugin will perform
     * this only for the first google talk account found
     * first, however, we must update the lastrun and changesmade
     * settings:
     * the lastrun setting will be the number of seconds since
     * Jan 1, 1970
     * the changesmade setting is the number of uploads and
     * merges performed
     */
    purple_prefs_set_int(
            "/plugins/core/google-contact/lastrun", time(NULL));
}

static xmlnode * getedituri(xmlnode *gcontact) {
    xmlnode *link;
    for (link = xmlnode_get_child(gcontact, "link");
         link != NULL;
         link = xmlnode_get_next_twin(link)) {
        if (strcmp(xmlnode_get_attrib(link, "rel"), "edit") == 0) {
            return link;
        }
    }
    /* if not found, there is a problem */
    return NULL;
}

static void upload(xmlnode *gcontact, char *auth, int len) {
    gchar *body, *request, *gcontactxml;
    gchar **split;
    const char *url = xmlnode_get_attrib(getedituri(gcontact), "href");
    /* len is the size of the downloaded file, so it will be used
     * for the upload size as well even though this should be way
     * more than necessary (this is a single contact's information)
     */
    int *length = &len;
    
    /* problem if url is null */
    if (url == NULL) return;

    /* convert xml to string
     * this requires conversion of &quot; back to "
     * this will be done by splitting the string at &quot; and then
     * putting it back together
     */
    gcontactxml = xmlnode_to_str(gcontact, length);
    split = g_strsplit(gcontactxml, "&quot;", 3);
    strcpy(gcontactxml, split[0]);
    strcat(gcontactxml, "\"");
    strcat(gcontactxml, split[1]);
    strcat(gcontactxml, "\"");
    strcat(gcontactxml, split[2]);
    g_strfreev(split);
    /* prepare the HTTP request */
    
    body = g_strdup_printf("<?xml version='1.0' encoding='UTF-8'?>\n%s",
            gcontactxml);
    request = g_strdup_printf("POST %s HTTP/1.0\r\n"
            "Host: www.google.com\r\n"
            "Authorization: GoogleLogin auth=%s\r\n"
            "Gdata-Version: 3.0\r\n"
            "If-Match: %s\r\n"
            "Content-Length: %d\r\n"
            "Content-Type: application/atom+xml\r\n"
            "X-HTTP-Method-Override: PUT\r\n\r\n"
            "%s", url, auth, xmlnode_get_attrib(gcontact, "etag"),
            strlen(body), body);
    purple_util_fetch_url_request(url, TRUE, USER_AGENT,
            FALSE, request, TRUE, uploadcb, NULL);
    g_free(body);
    g_free(request);
    g_free(gcontactxml);
}

/* callback function to login http request */
static void logincb(PurpleUtilFetchUrlData *url_data, gpointer user_data, const gchar *url_text, gsize len, const gchar *error_message) {
    gchar *auth, *url, *request;
    const char *host = "www.google.com";
    /* return if there was an http error */
    if (error_message != NULL) {
        /* if an error occured, the plugin was not successful */
        purple_prefs_set_bool("/plugins/core/google-contact/success",
                FALSE);
        return;
    }
    /* copy auth string from url_text http response */
    auth = g_strdup_printf("%s", strstr((char *) url_text, "Auth=") + 5);
    g_strchomp(auth);
    /* GET request to fetch all contacts */
    url = g_strdup_printf(
            "https://%s/m8/feeds/contacts/default/thin?max-results=%d",
            host, 10000);

    request = g_strdup_printf("GET %s HTTP/1.0\r\n"
            "Host: %s\r\n"
            "Authorization: GoogleLogin auth=%s\r\n"
            "Gdata-Version: 3.0\r\n\r\n", url, host, auth);
    
    purple_util_fetch_url_request(url, TRUE, USER_AGENT, FALSE,
            request, FALSE, getlistcb, auth);

    /* free memory */
    g_free(url);
    g_free(request);
}

/* callback function to download contact list GET request */
static void getlistcb(PurpleUtilFetchUrlData *url_data, gpointer user_data,
        const gchar *url_text, gsize len, const gchar *error_message) {
    gchar *auth = (gchar *)user_data;
    xmlnode *glist;
    GSList *buddies;
    int uploadcount = 0;
    if (error_message != NULL) {
        purple_prefs_set_bool("/plugins/core/google-contact/success",
                FALSE);
        return;
    }
    glist = xmlnode_from_str(url_text, strlen(url_text));
    /* update purple contacts with information from google contacts */
    purplemerger(glist);
    /* iterate through purple buddies */
    for (buddies = purple_blist_get_buddies();
         buddies != NULL;
         buddies = buddies->next) {
        PurpleBuddy *buddy = buddies->data;
       /* find a matching google contact for the buddy */
        xmlnode *gcontactmatch = findgooglecontact(buddy, glist);
        gboolean uploadneeded = FALSE;
        uploadneeded = updateglist(gcontactmatch,
            purple_buddy_get_contact(buddy));
        
        /* perform upload if necessary */
        if (uploadneeded) {
            upload(gcontactmatch, auth, len);
            /* if an upload was needed, update the upload counter */
            uploadcount ++;
        }
     }
    g_free(buddies);
    /* update the changes made with the number of uploads */
    purple_prefs_set_int("/plugins/core/google-contact/changesmade",
            purple_prefs_get_int(
                "/plugins/core/google-contact/changesmade")
            + uploadcount);
    
}

static xmlnode * findgooglecontact(PurpleBuddy *buddy, xmlnode *glist) {
    char *name = buddy->name;
    xmlnode *gcontact;
    /* iterate through all gcontacts */
    for (gcontact = xmlnode_get_child(glist, "entry");
         gcontact != NULL;
         gcontact = xmlnode_get_next_twin(gcontact)) {
        xmlnode *gIM;
        xmlnode *gemail;
        AccountType type = ptype(buddy->account);
        char *email;
        /* iterate through gIM (IM address) in gcontact to find match */
        for (gIM = xmlnode_get_child(gcontact, "im");
             gIM != NULL;
             gIM = xmlnode_get_next_twin(gIM)) {
            /* if they match, return gcontact */
            if ((strcmp(xmlnode_get_attrib(gIM, "address"), name) == 0)
                && (gtype(gIM) == type)) {
                return gcontact;
            }
        }

        /* iterate through emails to find match */
        /* for some protocols, a domain needs to be added to the username
         * to convert it to an email, hence the extra space in malloc
         */
        email = (char *)malloc(strlen(name) + 12);
        strcpy(email, name);
        if (strchr(email, '@') == NULL) {
            if (type == AIM) strcat(email, "@aol.com");
        }

        for (gemail = xmlnode_get_child(gcontact, "email");
                gemail != NULL;
                gemail = xmlnode_get_next_twin(gemail)) {
            if (!strcmp(xmlnode_get_attrib(gemail, "address"), email)) {
                free(email);
                return gcontact;
            }
            /* the google contacts service complains if a contact with
             * an empty email is uploaded
             */
            if (strcmp(xmlnode_get_attrib(gemail, "address"), "") == 0) {
                xmlnode_free(gemail);
            }

        }
        free(email);
    }
    
    /* no result was found */
    return NULL;
}

/* updates the google contacts xml IM fields  if necessary with info
 * from the purple contact
 * if this is necessary, then an upload is needed */
static gboolean updateglist(xmlnode *gcontact, PurpleContact *pcontact) {
    gboolean uploadneeded = FALSE;
    PurpleBlistNode *pnode = &(pcontact->node);
    xmlnode *gIM;
    /* if no gcontact was passed */
    if (gcontact == NULL) return FALSE;
    /* iterate through all buddies in the pcontact */
    for (pnode = pnode->child;
         pnode != NULL;
         pnode = pnode->next) {
        gboolean found = FALSE;
        const char *name = purple_blist_node_get_string(pnode, "name");
        AccountType type = purple_blist_node_get_int(pnode, "ptype");
        if (name == NULL) return FALSE; // this line prevents crashing
        /* iterate through all gIM's */
        for (gIM = xmlnode_get_child(gcontact, "im");
                gIM != NULL;
                gIM = xmlnode_get_next_twin(gIM)) {
            /* see if the the pidgin buddy appears as a gIM */
            if (!strcmp(name, xmlnode_get_attrib(gIM, "address"))
                    && (type == gtype(gIM))) {
                found = TRUE;
            }
        }

        /* if the purple buddy was not found in the google contact */
        if ((!found) && (type != POTHER)) {
            /* the gIM will need to be added to the google contact */
            xmlnode *newIM = xmlnode_new_child(gcontact, "gd:im");
            xmlnode_set_attrib(newIM, "address", name);
            xmlnode_set_attrib(newIM, "rel",
                    "http://schemas.google.com/g/2005#other");
            switch (type) {
                case GTALK: {xmlnode_set_attrib(newIM, "protocol",
                    "http://schemas.google.com/g/2005#GOOGLE_TALK");
                    break;}
                case AIM: {xmlnode_set_attrib(newIM, "protocol",
                    "http://schemas.google.com/g/2005#AIM");
                    break;}
                case YAHOO: {xmlnode_set_attrib(newIM, "protocol",
                    "http://schemas.google.com/g/2005#YAHOO");
                    break;}
                case MSN: {xmlnode_set_attrib(newIM, "protocol",
                    "http://schemas.google.com/g/2005#MSN");
                    break;}
                case QQ: {xmlnode_set_attrib(newIM, "protocol",
                    "http://schemas.google.com/g/2005#QQ");
                    break;}
                case JABBER: {xmlnode_set_attrib(newIM, "protocol",
                    "http://schemas.google.com/g/2005#JABBER");
                    break;}
                case ICQ: {xmlnode_set_attrib(newIM, "protocol",
                    "http://schemas.google.com/g/2005#ICQ");
                    break;}
                case SKYPE: {xmlnode_set_attrib(newIM, "protocol",
                    "http://schemas.google.com/g/2005#SKYPE");
                    break;}
                default : xmlnode_free(newIM);
            }
            /* this updated gcontact will need to be uploaded */
            uploadneeded = TRUE;
        }
    }

    /* if a change is made and an upload is needed, then some namespaces
     * need to be added or else google contacts will reject the upload
     */
    if (uploadneeded) {
        xmlnode_set_attrib(gcontact, "xmlns",
         "http://www.w3.org/2005/Atom");
        xmlnode_set_attrib(gcontact, "xmlns:gd",
            "http://schemas.google.com/g/2005");
        xmlnode_set_attrib(gcontact,
            "xmlns:gContact", "http://schemas.google.com/contact/2008");
    }
    return uploadneeded;
}

/* code to be first run when the plugin starts */
static gboolean plugin_load(PurplePlugin *plugin)
{
    /* declare variables */
    GSList *buddies;
    long int duration;
    void *accountshandle = purple_accounts_get_handle();
    /* if:
     * no change was made the last time the plugin was run
     * the plugin was run recently
     * the plugins last run was successful
     * then: the plugin will not be run
     *
     * duration is the number of days since the plugin was last run
     */
    duration = time(NULL) -
        purple_prefs_get_int("/plugins/core/google-contact/lastrun");
    if ((purple_prefs_get_int(
                "/plugins/core/google-contact/changesmade") == 0) &&
        /* number of days * 86400 seconds/day = number of days */
        (purple_prefs_get_int(
                "/plugins/core/google-contact/period") * 86400 > duration) &&
        purple_prefs_get_bool("/plugins/core/google-contact/success")) {
        return TRUE;
    }

    /* reset the changesmade counter for this run of the plugin */
    purple_prefs_set_int("/plugins/core/google-contact/changesmade", 0);
    purple_prefs_set_bool("/plugins/core/google-contact/success", TRUE);
    /* add "name" string and "ptype" int to the blist node hash tables
     * this needs to be done so that this information can be read from
     * the blist node
     */
    for (buddies = purple_blist_get_buddies();
         buddies != NULL;
         buddies = buddies->next) {
        PurpleBuddy *buddy = buddies->data;
        PurpleBlistNode *node = &(buddy->node);
        purple_blist_node_set_string(node, "name", buddy->name);
        purple_blist_node_set_int(node, "ptype", ptype(buddy->account));
    }
    g_free(buddies);
    
    purple_signal_connect(accountshandle, "account-signed-on", plugin, PURPLE_CALLBACK(sign_in_cb), NULL);

    
    return TRUE;
}

/* determinds the google IM account type */
static AccountType gtype(xmlnode *gIM) {
    /* declare variables */
    const char *proto = xmlnode_get_attrib(gIM, "protocol"); 
    if (proto == NULL) return GOTHER;
    else if (strstr(proto, "GOOGLE_TALK") != NULL) return GTALK;
    else if (strstr(proto, "AIM") != NULL) return AIM;
    else if (strstr(proto, "YAHOO") != NULL) return YAHOO;    
    else if (strstr(proto, "MSN") != NULL) return MSN;
    else if (strstr(proto, "QQ") != NULL) return QQ;
    else if (strstr(proto, "JABBBER") != NULL) return JABBER;
    else if (strstr(proto, "ICQ") != NULL) return ICQ;
    else if (strstr(proto, "SKYPE") != NULL) return SKYPE;
    else return GOTHER;
}

/* determines the purple account type */
static AccountType ptype(PurpleAccount *pAccount) {
    char *proto = pAccount->protocol_id;
    if (proto == NULL) return POTHER;
    if (!(strcmp(proto, "prpl-jabber")) &&
        (strstr(pAccount->username, "@gmail.com") != NULL)) {
        return GTALK;
    }
    else if (!strcmp(proto, "prpl-aim")) return AIM;
    else if (!strcmp(proto, "prpl-yahoo")) return YAHOO;
    else if (!strcmp(proto, "prpl-msn")) return MSN;
    else if (!strcmp(proto, "prpl-qq")) return QQ;
    else if (!strcmp(proto, "prpl-jabber")) return JABBER;
    else if (!strcmp(proto, "prpl-icq")) return ICQ;
    else if (!strcmp(proto, "prpl-bigbrownchunx-skype")) return SKYPE;
    else return POTHER;
}

/* updates purple contact list using gxml file */
static void purplemerger(xmlnode *glist) {
    /* sync from google contacts to libpurple
     * this will involve the following steps: 
     *  1) iterate through google contacts
            a) for first gIM in gcontact, find equiv pcontact
     *      b) iterate through subsequent gIM in gcontact
     *          i) find equivalent pIM for each gIM
     *          ii) if not in same pcontact, merge all of pIM's
     */
    
    xmlnode *gContact;
    GList *pAccountList;
    int mergecount = 0;
    
    /* iterate through google contacts */
    for (gContact = xmlnode_get_child(glist, "entry");
         gContact != NULL;
         gContact = xmlnode_get_next_twin(gContact))    {
        xmlnode *gIM;
        PurpleBuddy *buddy;
        PurpleContact *contact;
        /* will hold 1st pcontact found, others will be merged with it */
        PurpleContact *pMainContact = NULL;
        PurpleBlistNode *pMergeNode = NULL;
        
        /* iterate through gIM's in gcontact */
        for (gIM = xmlnode_get_child(gContact, "im");
                gIM != NULL;
                gIM = xmlnode_get_next_twin(gIM))  {
            /* check if gIM has equiv pIM in a pContact
             * will require iterating through pContacts */
            
            /* iterate through purple accounts */
            for (pAccountList = purple_accounts_get_all();
                 pAccountList != NULL;
                 pAccountList = pAccountList->next)  {
                
                /* initialize pAccount from pAcccountList data */
                PurpleAccount *pAccount = (PurpleAccount *)pAccountList->data;
                /* check if the account types are the same */
                if (ptype(pAccount) == gtype(gIM)) {
                    /* find pIM with the same name as gIM */
                    GList *foundBuddies;
                    /* iterate through found buddies */
                    for (foundBuddies = (GList *) purple_find_buddies(
                            pAccount, xmlnode_get_attrib(gIM, "address"));
                         foundBuddies != NULL;
                         foundBuddies = foundBuddies->next) {
                         /* set buddy to data from GList struct */
                         buddy = (PurpleBuddy *)foundBuddies->data;
                         
                         /* if this is the first contact found, save it as
                          * pMainContact so future finds can be merged in it
                          */
                         if (pMainContact == NULL) {
                            pMainContact = purple_buddy_get_contact(buddy);
                            pMergeNode = &buddy->node;
                        }
                        /* if this is not the first contact */
                        else {
                            contact = (PurpleContact *) purple_buddy_get_contact(buddy);
                            
                            /*  if the contacts are different, merge */
                            if (contact != pMainContact) {
                                purple_blist_merge_contact(contact, pMergeNode);
                                /* update the merge counter  */
                                mergecount ++;
                            }
                        }
                     }
                }
            }
        }
    }
    /* update the changecount with the number of merges */
    purple_prefs_set_int("/plugins/core/google-contact/changesmade",
        purple_prefs_get_int("/plugins/core/google-contact/changesmade")
            + mergecount);
}

static PurplePluginInfo info = {
    PURPLE_PLUGIN_MAGIC,
    PURPLE_MAJOR_VERSION,
    PURPLE_MINOR_VERSION,
    PURPLE_PLUGIN_STANDARD,
    NULL,
    0,
    NULL,
    PURPLE_PRIORITY_DEFAULT,

    "core-google-contact",
    "Google Contacts Integration",
    "1.04",

    "Syncs buddy list with google contacts",          
    "This plugin will synchronize the buddy list with the IM fields in google contacts",          
    "noobster721@gmail.com",                          
    "http://code.google.com/p/pidgin-google-contact/",     
    
    plugin_load,
    NULL,
    NULL,                          
                                   
    NULL,                          
    NULL,                          
    NULL,                        
    NULL,                   
    NULL,                          
    NULL,                          
    NULL,                          
    NULL                           
};                               
    
static void init_plugin(PurplePlugin *plugin)
{
    purple_prefs_add_none("/plugins/core/google-contact");
    purple_prefs_add_int("/plugins/core/google-contact/changesmade", -1);
    purple_prefs_add_int("/plugins/core/google-contact/lastrun", -1);
    purple_prefs_add_int("/plugins/core/google-contact/period", 5);
    purple_prefs_add_bool("/plugins/core/google-contact/success", TRUE);
}

PURPLE_INIT_PLUGIN(google-contact, init_plugin, info)
