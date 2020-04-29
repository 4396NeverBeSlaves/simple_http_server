#include<stdio.h>
#include<string.h>

int hex_dec(char c){
    if(c<='9')
        return c-'0';
    else
        return c<='F'?c-'A'+10:c-'a'+10;
}
int url_decode(char *url){
    int i,j,src_len;

    src_len=strlen(url);
    char res[src_len];
    for ( i = 0,j=0; i < src_len; i++,j++)
    {
        if(url[i]=='%'){
            int high4=hex_dec(url[i+1]);
            int low4=hex_dec(url[i+2]);
            res[j]=high4*16+low4;
            i+=2;
        }else if(url[i]=='+'){
            res[j]=' ';
        }else{
            res[j]=url[i];
        }
        
    }
    res[j]='\0';
    
    for ( i = 0; i < j; i++){
        url[i]=res[i];
    }
    url[i]='\0';
    return j;
}
