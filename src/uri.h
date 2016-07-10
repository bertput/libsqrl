#ifndef SQRL_URI_H_INCLUDED
#define SQRL_URI_H_INCLUDED

#ifndef DLL_PUBLIC
#define DLL_PUBLIC _declspec(dllimport)
#endif

#include "sqrl_common.h"

/** 
 * Parses a URL and stores the parts that libsqrl needs.
 */
class DLL_PUBLIC SqrlUri
{
public:
	SqrlUri(const char *source);
	~SqrlUri();

	Sqrl_Scheme getScheme();

	/** The Challenge is the full, original URL, or the response body from a previous SQRL transaction */
	char *getChallenge();
	size_t getChallengeLength();
	void setChallenge(const char *val);

	/** The Hostname (fqdn) */
	char *getHost();
	size_t getHostLength();

	/** The portion of the URL that the Site Specific Keys are based on.
	  * Typically, the FQDN, followed by an optional extension.
	  */
	char *getPrefix();
	size_t getPrefixLength();

	/** The server URL for the next transaction */
	char *getUrl();
	size_t getUrlLength();
	void setUrl(const char *val);

	/** The 'Server Friendly Name' */
	char* getSFN();
	size_t getSFNLength();

	/** Creates a copy of a SqrlUri object. */
	SqrlUri* copy();

private:
	Sqrl_Scheme scheme;
	char *challenge;
	char *host;
	char *prefix;
	char *url;
	char *sfn;
};


#endif //SQRL_URI_H_INCLUDED