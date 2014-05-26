#ifndef _EPower_Message_hh__
#define _EPower_Message_hh__

#include <string>
#include <vector>
#include <map>
#ifdef WIN32
#	pragma warning(disable: 4251)
#endif // 

#ifdef _LIB
# define HTTPAPI
#else
#ifdef WIN32
#	ifdef HTTPSRV_EXPORTS
#		define HTTPAPI __declspec(dllexport)
#	else
#		define HTTPAPI __declspec(dllimport)
#	endif // HTTPSRV_EXPORTS 
#else
#	define HTTPAPI
#endif // win32
#endif // _LIB


namespace EPower {

	class MessageParser;

	/** url http://192.168.1.111:1554/go7007_0/command?brightness=80&saturation=65
	*/
	struct HTTPAPI Url
	{
		std::string schema;	    // http
		std::string authority;	    // 192.168.1.111
		std::string path;	    // go7007/command
		std::string query;	    // brightness=80&...
		std::string fragment;	    // #...

		std::vector<std::pair<std::string, std::string> > params; // param1 brightness=80
		// param2 saturation=65
		//
		static Url parseUrl (const char *url);

		virtual ~Url ();
	};

	struct HTTPAPI Url2
	{
		char *schema;	    // http
		char *authority;	    // 192.168.1.111
		char *path;	    // go7007/command
		char *query;	    // brightness=80&...
		char *fragment;	    // #...
		
		static Url2 *parseUrl (const char *url);
		static void free (Url2 *p);

		virtual ~Url2();
		Url2 ();
	};

	class HTTPAPI Message
	{
		friend class MessageParser;

	public:
		enum State
		{
			HEADERS,
			BODY,
			COMPLETE,
			CR,
			CRLF,
			LSW,
		};

		Message ();
		virtual ~Message ();

		void setStartLine (const char *p1, const char *p2, const char *p3);

		const char *p1 () const;
		const char *p2 () const;
		const char *p3 () const;
		std::string safe_p2 () const;	// 转换url中的 %XX

		void update_url (const char *newurl);

		bool getValue (const char *key, const char **value) const;
		bool setValue (const char *key, const char *value);
		void removeValue (const char *key);

		bool getBody (const char **body, size_t *size) const;
		bool appendBody (const char *body, size_t size);
		bool clearBody ();

		void encode (std::ostream &sout) const;

		bool isComplete () const;

		void close();	// release me

		State state () const { return m_state; }

		/// ???ػ???Ҫ???յ? body ???ֽڳ???
		size_t residualBodyLength () const;
		/// ???? Content-Length ָ???ĳ???
		size_t getContentLength () const;

		bool m_requestline;
	
		State m_state, m_last_state;
		std::string m_tmpline;
		std::string m_body;
		std::string m_p1, m_p2, m_p3;
		std::map<std::string, std::string> m_headers;

		int appendByte (char c, const MessageParser *parser);

		size_t appendBodyData (const char *data, size_t len,
			const MessageParser *parser);
	};

	class HTTPAPI MessageParser
	{
	public:
		static Message *parse (const MessageParser *parser,
			const char *data, size_t len,
			size_t *used, Message *savedmsg);

		static void releaseMsg (Message *msg);

		virtual ~MessageParser () {}

		virtual Message *createMessage() const = 0;

		virtual size_t getMessageBodyLength (const Message *msg) const = 0;

		virtual bool parseHeader (std::string &line,
			Message *msg) = 0;
	};

	class HttpAuthen
	{
	public:
		static bool digestRequest (char *buf,
			const char *auth_header,
			const char *uri,
			const char *user, const char *passwd,
			const char *cnonce,
			int nc);

	};
}; // namespace EPower

#endif // _EPower_Message_hh__
