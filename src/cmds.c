#include "cmds.h"

static char newsrv_buf[BUF_SIZE];

static const char *defaultConfig_contents =
"# nu default config\n"
"# for help, please visit https://github.com/nu-dev/nu/wiki/Getting-Started\n"
"theme = \"basic\"\n"
"sitename = \"My new blog\"\n"
"sitedesc = \"My new blog!\"\n";

int newSrv(char *name) {
    #define DIRSTOMAKECOUNT 6
    int nameLen;
    int cwdLen;
    int i;
    char *dirsToMake[DIRSTOMAKECOUNT] = {"", "posts", "raw", "resources", "special", "theme"};
    char *newDirName;
    FILE *fp;
    
    nameLen = strlen(name);
    cwdLen = 0;
    
    if(getCurrDir(newsrv_buf, BUF_SIZE - nameLen - NU_CONFIG_NAME_LENGTH) == -1) {
        return -1;
    }
    cwdLen = strlen(newsrv_buf);

    /* copy over the project name */
    newsrv_buf[cwdLen] = '/';
    for (i = 0; i <= nameLen; i++)
        newsrv_buf[cwdLen+i+1] = name[i];
    
    /* make sure directory doesn't already exist */
    if (dirExists(newsrv_buf)) {
        fprintf(stderr, "["KRED"ERR"RESET"] Directory %s already exists!\n", newsrv_buf);
        return -1;
    }
    
    for (i = 0; i < DIRSTOMAKECOUNT; i++) { /* loop through all the directories to make */
        printf("["KBLU"INFO"RESET"] Making directory %s/%s!\n", newsrv_buf, dirsToMake[i]);
        newDirName = dirJoin(newsrv_buf, dirsToMake[i]);
        if (makeNewDir(newDirName) == 0) { /* make directory itself */
            fprintf(stderr, "["KRED"ERR"RESET"] Failed  to make directory %s/%s! (Error #%d)\n", newsrv_buf, dirsToMake[i], errno);
            free(newDirName);
            return -1;
        }
        free(newDirName);
    }
    
    /* copy over config name */
    newsrv_buf[cwdLen+nameLen+1] = '/';
    strncpy(&newsrv_buf[cwdLen+nameLen+2], NU_CONFIG_NAME, NU_CONFIG_NAME_LENGTH);

    /* create the file */
    fp = fopen(newsrv_buf, "w");
    if (fp == NULL) {
        fprintf(stderr, "["KRED"ERR"RESET"] Failed to open file %s! (Error #%d)\n", newsrv_buf, errno);
        return -1;
    }
    
    /* write into it */
    fputs(defaultConfig_contents, fp);
    fclose(fp);
    
    #undef DIRSTOMAKECOUNT
    return 0;
}

int cleanNuDir(char *nuDir) {
    char *removingDir;
    int hasErr;
    
    hasErr = 0;
    
    removingDir = dirJoin(nuDir, "posts");
    printf("["KBLU"INFO"RESET"] Deleting directory %s...\n", removingDir);
    if (delDir(removingDir) != 0) {
        fprintf(stderr, "["KRED"ERR"RESET"] Failed to clear directory %s! Check if you have permissions to delete.\n", removingDir);
        hasErr = 1;
        goto end;
    }
    freeThenNull(removingDir);
    
    removingDir = dirJoin(nuDir, "special");
    printf("["KBLU"INFO"RESET"] Deleting directory %s...\n", removingDir);
    if (delDir(removingDir) != 0) {
        fprintf(stderr, "["KRED"ERR"RESET"] Failed to clear directory %s! Check if you have permissions to delete.\n", removingDir);
        hasErr = 1;
        goto end;
    }
    freeThenNull(removingDir);
    
    removingDir = dirJoin(nuDir, "page");
    printf("["KBLU"INFO"RESET"] Deleting directory %s...\n", removingDir);
    if (delDir(removingDir) != 0) {
        fprintf(stderr, "["KRED"ERR"RESET"] Failed to clear directory %s! Check if you have permissions to delete.\n", removingDir);
        hasErr = 1;
        goto end;
    }
    freeThenNull(removingDir);
    
    removingDir = dirJoin(nuDir, "index.html");
    printf("["KBLU"INFO"RESET"] Deleting file %s...\n", removingDir);
    if (remove(removingDir) != 0) {
        fprintf(stderr, "["KYEL"WARN"RESET"] Failed to clear file %s! It may already not exist. If it does, check if you have permissions to delete.\n", removingDir);
    }
    freeThenNull(removingDir);
end:
    free(removingDir);
    return hasErr;
}

char *getNuDir(int argc, char** argv) {
    char *buf;
    char *nuDir;
    
    buf = calloc(BUF_SIZE, sizeof(char));
    
    if (argc == 2) { /* no other arguments passed, assume user means current directory is nudir */
        if (getCurrDir(buf, BUF_SIZE) != 0) return NULL;
        
        nuDir = buf;
    } else if (argc == 3) { /* specified a directory to use as nudir */
        if (getCurrDir(buf, BUF_SIZE) != 0) return NULL;
        nuDir = dirJoin(buf, argv[2]);
        free(buf);
    } else {
        fprintf(stderr, "["KRED"ERR"RESET"] Invalid number of arguments passed. For help, use `%s help`.\n", *argv);
        free(buf);
        return NULL;
    }
    if (!isNuDir(nuDir)) goto notnudir;
    fprintf(stderr, "["KBLU"INFO"RESET"] Using `%s` as the nu directory.\n", nuDir);
    return nuDir;
notnudir:
    fprintf(stderr, "["KRED"ERR"RESET"] The specified directory %s is not a valid nu directory. Please check that the file `"NU_CONFIG_NAME"` exists and try again.\n", nuDir);
    free(nuDir);
    return NULL;
}

extern char *globNuDir;
static char *normal_template;
static char *special_template;
static char *index_template;
static char *singlepost_template;
static char *navbar_template;
static post_list *pl = NULL;
static str_list *sl = NULL;
static post_frag_list *pfl = NULL;
static post_frag_list *sfl = NULL;
static map_t combined_dic;

int builderHelper(const char *inFile) {
    static post *temp;
    static const char *ext;

    ext = fileExtension(inFile);
    if (strcmp(ext, "md") != 0 && strcmp(ext, "markdown") != 0) {
        printf("["KYEL"WARN"RESET"] Skipping file %s - file extension is not `md` or `markdown`.\n", inFile);
        return 0;
    }

    temp = post_create(inFile);
    if (temp == NULL) {
        return -1;
    }
    pl_add_post(pl, temp);
	return 0;
}

int buildNuDir(char *nuDir) {
    char *buildingDir = NULL, *configContents = NULL, *cfgfname = NULL,
         *temp = NULL, *themedir = NULL, *templated_output = NULL,
         *temp2 = NULL, *currpage = NULL, *lastPage = NULL,
         *currPageOut = NULL, *navbarText = NULL,
         *theme = NULL;
    char pagenum_buf[16], pagenum_buf2[22], currpagenum_buf[11];
    int ok;
    unsigned int pagenum, i, maxPostsPerPage;
    map_t global_dic = NULL, theme_dic = NULL,
          currpost_dic = NULL, temp_dic = NULL;
    post_list_elem *currPost = NULL;
    post_frag_list_elem *currFrag = NULL;
    void *tmp;

    buildingDir = dirJoin(nuDir, "raw");
    globNuDir = nuDir;
    pl = pl_new();
    sl = sl_new();
    pfl = pfl_new();
    sfl = pfl_new();
    
    printf("["KBLU"INFO"RESET"] Starting to build the directory `%s`!\n", nuDir);
    
    /* parse global config */
    printf("["KBLU"INFO"RESET"] Parsing global nu config...\n");
    global_dic = hashmap_new();
    cfgfname = dirJoin(nuDir, NU_CONFIG_NAME);
    configContents = dumpFile(cfgfname);
    
    if (configContents == NULL) {
        fprintf(stderr, "["KRED"ERR"RESET"] Could not open the file `%s`.\n", cfgfname);
        ok = 0;
        goto end;
    }
    
    ok = parse_config(configContents, "", global_dic);
    if (!ok) {
         fprintf(stderr, "["KRED"ERR"RESET"] Errors occured while parsing the global config. Please check them and try again.\n");
         goto end;
    }
    freeThenNull(configContents);
    
    /* check if theme config specified */
    if (hashmap_get(global_dic, "theme", &tmp) == MAP_MISSING) {
        fprintf(stderr, "["KRED"ERR"RESET"] Could not find a key of name `theme` to determine what theme nu is going to use. Please see https://github.com/nu-dev/nu/wiki/Getting-Started for help.\n");
        ok = 0;
        goto end;
    }
    theme = (char *)tmp;
    
    temp = dirJoin(nuDir, "theme");
    themedir = dirJoin(temp, theme);
    freeThenNull(temp);
    printf("["KBLU"INFO"RESET"] Parsing theme config from %s...\n", themedir);
    
    /* check if theme exists */
    if (!(dirExists(themedir) && isNuDir(themedir))) {
        fprintf(stderr, "["KRED"ERR"RESET"] The theme `%s` could not be found! Please make sure it is in the %s directory and has the file `"NU_CONFIG_NAME"`.\n", theme, themedir);
        ok = 0;
        goto end;
    }
    
    /* read theme config */
    theme_dic = hashmap_new();
    temp = dirJoin(themedir, NU_CONFIG_NAME);
    configContents = dumpFile(temp);
    freeThenNull(temp);
    ok = parse_config(configContents, "theme.", theme_dic);
    if (!ok) {
        fprintf(stderr, "["KRED"ERR"RESET"] Errors occured while parsing the theme config. Please check them and try again.\n");
        ok = 0;
        goto end;
    }
    freeThenNull(configContents);
    
    /* read the index page for the theme */
    printf("["KBLU"INFO"RESET"] Reading index page template...\n");
    temp = dirJoin(themedir, "index.html");
    index_template = dumpFile(temp);
    if (index_template == NULL) {
        fprintf(stderr, "["KRED"ERR"RESET"] The theme file `%s` could not be found! Please make sure it is in the %s directory.\n", temp, themedir);
        ok = 0;
        goto end;
    }
    freeThenNull(temp);
    
    /* read the post page for the theme */
    printf("["KBLU"INFO"RESET"] Reading post page template...\n");
    temp = dirJoin(themedir, "post.html");
    normal_template = dumpFile(temp);
    if (normal_template == NULL) {
        fprintf(stderr, "["KRED"ERR"RESET"] The theme file `%s` could not be found! Please make sure it is in the %s directory.\n", temp, themedir);
        ok = 0;
        goto end;
    }
    freeThenNull(temp);
    
    /* read the special page for the theme */
    printf("["KBLU"INFO"RESET"] Reading special page template...\n");
    temp = dirJoin(themedir, "special.html");
    special_template = dumpFile(temp);
    if (special_template == NULL) {
        fprintf(stderr, "["KRED"ERR"RESET"] The theme file `%s` could not be found! Please make sure it is in the %s directory.\n", temp, themedir);
        ok = 0;
        goto end;
    }
    freeThenNull(temp);
    
    /* combine the two dictionaries */
    combined_dic = hashmap_merge(global_dic, theme_dic);
    
    
    
    printf("["KBLU"INFO"RESET"] Creating list of posts...\n");
    /* loop through dir and do all the stuff*/
    if (loopThroughDir(buildingDir, &builderHelper) != 0) {
        fprintf(stderr, "["KRED"ERR"RESET"] Failed to build! Check if you have permissions to read files in `%s/raw` and try again.\n", nuDir);
        ok = 0;
        goto end;
    }
    
    if (pl->length == 0) {
        printf("["KYEL"WARN"RESET"] There is nothing to build.\n");
        return 0;
    }
    pl_sort(&pl);
    
    currPost = pl->head;
    while (currPost != NULL) {
        if (sl_exists_inside(sl, (currPost->me)->out_loc)) {
            fprintf(stderr, "["KRED"ERR"RESET"] There are two posts with the same output location to `%s`. Please see https://github.com/nu-dev/nu/wiki/Duplicate-output-posts for more help.\n", (currPost->me)->out_loc);
            goto end;
        } else {
            sl_add_post(sl, (currPost->me)->out_loc);
        }
        currPost = currPost->next;
    }

    /* read the navbar fragment for the theme */
    printf("["KBLU"INFO"RESET"] Reading navbar template from navbar_template fragment...\n");
    if (hashmap_get(combined_dic, "theme.navbar_template", &tmp) == MAP_MISSING) {
        fprintf(stderr, "["KYEL"WARN"RESET"] Could not find a key of name `navbar_template` in the theme config. Assuming no navbar is needed.\n");
        navbar_template = NULL;
        goto done_nav;
    }
    navbar_template = strdup((char *)tmp);
    _okhere();
    
    currPost = pl->head;
    /* get list of special pages */
    while (currPost != NULL) {
        if ((currPost->me)->is_special) {
            temp_dic = hashmap_new();
            _okhere();
            hashmap_put(temp_dic, "post.name", (currPost->me)->name);
            hashmap_put(temp_dic, "post.contents", (currPost->me)->contents);
            hashmap_put(temp_dic, "post.mdate", (currPost->me)->mdate);
            hashmap_put(temp_dic, "post.mtime", (currPost->me)->mtime);
            hashmap_put(temp_dic, "post.in_fn", (currPost->me)->in_fn);
            hashmap_put(temp_dic, "post.out_loc", (currPost->me)->out_loc);
            hashmap_put(temp_dic, "post.raw_link", (currPost->me)->raw_link);
            currpost_dic = hashmap_merge(combined_dic, temp_dic);
            
            temp = calcPermalink((currPost->me)->out_loc);
            hashmap_put(currpost_dic, "post.link", temp);
            freeThenNull(temp);
            
            /* double pass */
            _okhere();
            
            temp = parse_template(navbar_template, currpost_dic);
            _okhere();
            temp2 = parse_template(temp, currpost_dic);
            _okhere();
            freeThenNull(temp);
            
            /* add post fragment */
            pfl_add(sfl, temp2);
            freeThenNull(temp2);
        }
        currPost = currPost->next;
    }
    _okhere();
    /* generate the navbar into the key `special.navbar` */
    currFrag = sfl->head;
    while (currFrag != NULL) {
        if (navbarText == NULL) {
            navbarText = strdup(currFrag->frag);
        } else {
            temp = strutil_append_no_mutate(navbarText, currFrag->frag);
            freeThenNull(navbarText);
            navbarText = temp;
        }
        currFrag = currFrag->next;
    }
    hashmap_put(combined_dic, "special.navbar", navbarText);
    _okhere();
    done_nav:
    
    /* read the singlepost fragment for the theme */
    printf("["KBLU"INFO"RESET"] Reading single post template from singlepost_template fragment...\n");
    if (hashmap_get(combined_dic, "theme.singlepost_template", &tmp) == MAP_MISSING) {
        fprintf(stderr, "["KYEL"WARN"RESET"] Could not find a key of name `singlepost_template` in the theme config. Assuming no pages are needed.\n");
        singlepost_template = NULL;
    } else {
        singlepost_template = strdup((char *)tmp);
    }
    
    /* loop through the entire list */
    currPost = pl->head;
    while (currPost != NULL) {
        /* populate the dictionary */
        temp_dic = hashmap_new();
        hashmap_put(temp_dic, "post.name", (currPost->me)->name);
        hashmap_put(temp_dic, "post.contents", (currPost->me)->contents);
        hashmap_put(temp_dic, "post.cdate", (currPost->me)->cdate);
        hashmap_put(temp_dic, "post.mdate", (currPost->me)->mdate);
        hashmap_put(temp_dic, "post.mtime", (currPost->me)->mtime);
        hashmap_put(temp_dic, "post.in_fn", (currPost->me)->in_fn);
        hashmap_put(temp_dic, "post.out_loc", (currPost->me)->out_loc);
        
        temp = (currPost->me)->raw_link;
        temp = dirJoin(hashmap_get_default(combined_dic, "linkprefix", ""), temp);
        hashmap_put(temp_dic, "post.raw_link", temp);
        free(temp);
        
        _okhere();
        
        currpost_dic = hashmap_merge(combined_dic, temp_dic);
        
        _okhere();
        
        if ((currPost->me)->is_special) {
            templated_output = parse_template(special_template, currpost_dic);
            
            temp = calcPermalink((currPost->me)->out_loc);
            hashmap_put(currpost_dic, "post.link", temp);
            freeThenNull(temp);
        } else {
            templated_output = parse_template(normal_template, currpost_dic);

            temp = calcPermalink((currPost->me)->out_loc);
            hashmap_put(currpost_dic, "post.link", temp);
            freeThenNull(temp);
            
            _okhere();
            
            /* double pass */
            if (singlepost_template) {
                _okhere();

                temp = parse_template(singlepost_template, currpost_dic);
                temp2 = parse_template(temp, currpost_dic);
                freeThenNull(temp);
                
                _okhere();
                
                /* add post fragment */
                pfl_add(pfl, temp2);
                freeThenNull(temp2);
            }
        }
        
        _okhere();
        
        if ((currPost->me)->delta_time <= 0) { /* skip this post if output file hasn't changed */
            goto nextpost;
        }
        printf("["KBLU"INFO"RESET"] Building file %s to %s...\n", (currPost->me)->in_fn, (currPost->me)->out_loc);
        
        _okhere();
        
        /* double pass */_okhere();
        temp = templated_output;_okhere();
        templated_output = parse_template(temp, currpost_dic);_okhere();
        freeThenNull(temp);
        _okhere();
        ok = writeFile((currPost->me)->out_loc, templated_output) + 1;
        _okhere();
        if (!ok) {
            hashmap_free(temp_dic);
            goto end;
        }
        _okhere();
    nextpost:
        /* clean up */
        freeThenNull(currpost_dic);
        hashmap_free(temp_dic);
        currPost = currPost->next;
        _okhere();
    }
    _okhere();
    
    if (singlepost_template == NULL) {
        /* no pagination needed */
        /* double pass */
        temp = parse_template(index_template, combined_dic);
        templated_output = parse_template(temp, combined_dic);
        freeThenNull(temp);
        
        temp = dirJoin(nuDir, "index.html");
        
        printf("["KBLU"INFO"RESET"] Writing index file to `index.html`...\n");
        
        /* write the index.html */
        ok = writeFile(temp, templated_output) + 1;
        if (!ok) {
            goto end;
        }
        freeThenNull(temp);
        goto done_pages;
    }
    
    /* create all the pages */
    /* check if theme config max posts per page */
    if (hashmap_get(theme_dic, "theme.maxpostsperpage", &tmp) == MAP_MISSING || (maxPostsPerPage = atoi((char *)tmp)) == 0) {
        printf("["KYEL"WARN"RESET"] The theme `%s` does not specify `maxpostsperpage`, so the default value of 3 is being used instead.\n", theme);
        maxPostsPerPage = 3;
    }
    
    /* calculate number of pages */
    sprintf(currpagenum_buf, "%d", (pfl->length)/maxPostsPerPage + (((pfl->length)%maxPostsPerPage == 0)?0:1));
    hashmap_put(combined_dic, "pagination.totalpages", currpagenum_buf);
    
    currFrag = pfl->head;
    i = 1;
    pagenum = 1;
    currpage = NULL;
    
    while (currFrag != NULL) {
        /* loop through all the post fragments */
        
        /* concatenate this fragment to the current string */
        if (currpage == NULL) {
            currpage = strdup(currFrag->frag);
        } else {
            temp = strutil_append_no_mutate(currpage, currFrag->frag);
            freeThenNull(currpage);
            currpage = temp;
        }
        
        /* if this is the last post on the page (postsThisPage = maxPostsPerPage)
           OR if the next page fragment == NULL
           then dump the current string to the page `n` */
        if (i == maxPostsPerPage || currFrag->next == NULL) {
            /* get page output */
            temp = dirJoin(nuDir, "page");
            sprintf(pagenum_buf, "%d.html", pagenum);
            currPageOut = dirJoin(temp, pagenum_buf);
            freeThenNull(temp);
            printf("["KBLU"INFO"RESET"] Building page %d to %s...\n", pagenum, currPageOut);
            
            /* get currpagenum as string */
            sprintf(currpagenum_buf, "%d", pagenum);
            
            /* temp post dic */
            temp_dic = hashmap_new();
            hashmap_put(temp_dic, "pagination.currpage", currpage);
            hashmap_put(temp_dic, "pagination.currpagenum", currpagenum_buf);
            
            /* merge the dics */_okhere();
            currpost_dic = hashmap_merge(combined_dic, temp_dic);

            
            /* get last page link */_okhere();
            if (lastPage == NULL) { /* last page was null (aka this is first page) */
                
            } else {
                /* not first page - there are newer pages*/
                #define PAGINATION_NEWER_LINK "_pagination.newerLink"
                temp = calcPermalink(lastPage);
                hashmap_put(currpost_dic, PAGINATION_NEWER_LINK, temp);
                freeThenNull(temp);_okhere();
                temp2 = parse_template("<a class=\"{{theme.newerlinkclass}}\" href=\"{{linkprefix}}/{{"PAGINATION_NEWER_LINK"}}\">{{theme.newerlinktext}}</a>", currpost_dic);
                hashmap_remove(currpost_dic, PAGINATION_NEWER_LINK);
                _okhere();hashmap_put(currpost_dic, "pagination.newer_link", temp2);
                freeThenNull(temp2);
                freeThenNull(lastPage);
                #undef PAGINATION_NEWER_LINK
            }
            lastPage = strdup(currPageOut);_okhere();
            
            /* get next page link */
            /* calculate if this is last page */
            if (currFrag->next == NULL) {
                
            } else {_okhere();
                /* not last page - there are older pages */
                sprintf(pagenum_buf2, "/page/%d.html", pagenum+1); /* plus 1 for next */
                #define PAGINATION_OLDER_LINK "_pagination.olderLink"
                hashmap_put(currpost_dic, PAGINATION_OLDER_LINK, pagenum_buf2);
                temp2 = parse_template("<a class=\"{{theme.olderlinkclass}}\" href=\"{{linkprefix}}{{"PAGINATION_OLDER_LINK"}}\">{{theme.olderlinktext}}</a>", currpost_dic);
                hashmap_remove(currpost_dic, PAGINATION_OLDER_LINK);
                hashmap_put(currpost_dic, "pagination.older_link", temp2);
                freeThenNull(temp2);
                #undef PAGINATION_OLDER_LINK
            }_okhere();

            /* double pass */
            temp = parse_template(index_template, currpost_dic);_okhere();
            templated_output = parse_template(temp, currpost_dic);
            freeThenNull(temp);
            
            /* write the /page/<pagenum> */
            ok = writeFile(currPageOut, templated_output) + 1;
            if (!ok) {
                hashmap_free(temp_dic);
                goto end;
            }
            hashmap_free(temp_dic);
            
            _okhere();
            /* also write the index.html if this is the first page */
            if (pagenum == 1) {
                temp = dirJoin(nuDir, "index.html");
                ok = writeFile(temp, templated_output) + 1;
                if (!ok) {
                    goto end;
                }
                freeThenNull(temp);
            }
            
            _okhere();
            
            i = 1;
            pagenum++;
            freeThenNull(currpage);
            freeThenNull(currpost_dic);
            freeThenNull(templated_output);
            freeThenNull(currPageOut);
        } else {
            i++;
        }
        currFrag = currFrag->next;
    }
    
    done_pages:
    end:/*
    fflush(stdout);
    if (pl) pl_clean(pl);
    if (sl) sl_clean(sl);
    if (pfl) pfl_clean(pfl);
    if (sfl) pfl_clean(sfl);
    free(buildingDir);
    free(configContents);
    free(cfgfname);
    free(temp);
    free(temp2);
    free(themedir);
    free(currpage);
    free(lastPage);
    free(navbarText);
    free(currPageOut);
    free(theme_dic);
    free(global_dic);
    free(templated_output);
    free(normal_template);
    free(special_template);
    free(index_template);
    free(singlepost_template);
    free(navbar_template);*/
    return !ok;
}
