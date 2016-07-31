#include "stdafx.h"
#include "CppUnitTest.h"

#include "sqrl.h"
#include "Sqrluri.h"
#include "NullClient.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace libsqrltest
{		
	TEST_CLASS(UriTests)
	{
	public:
		TEST_CLASS_INITIALIZE(InitializeSqrl)
		{
			char v[64];
			Sqrl_Version( v, 64 );
			std::string str( "UriTests: " );
			str.append( v );
			Logger::WriteMessage( str.data() );
		}
		
		void testString(char *a, const char *b) {
			Assert::AreEqual(a, b);
			if(a) free(a);
		}

		TEST_METHOD(Uri1)
		{
			new NullClient();
			SqrlUri *uri = SqrlUri::parse("sqrl://sqrlid.com/login?x=6&nut=blah&sfn=U1FSTGlk");
			Assert::IsNotNull( uri );
			Assert::IsTrue(uri->getScheme() == SQRL_SCHEME_SQRL);
			this->testString(uri->getSiteKeyString(), "sqrlid.com/login");
			this->testString(uri->getChallenge(), "sqrl://sqrlid.com/login?x=6&nut=blah&sfn=U1FSTGlk");
			this->testString(uri->getUrl(), "https://sqrlid.com/login?x=6&nut=blah&sfn=U1FSTGlk");
			this->testString(uri->getPrefix(), "https://sqrlid.com");
			this->testString(uri->getSFN(), "SQRLid");
			uri->release();
			delete (NullClient*)NullClient::getClient();
		}
		
		TEST_METHOD(Uri2)
		{
			new NullClient();
			SqrlUri *uri = SqrlUri::parse("sqrl://sqrlid.com/login?nut=blah&sfn=U1FSTGlk");
			Assert::IsNotNull( uri );
			Assert::IsTrue(uri->getScheme() == SQRL_SCHEME_SQRL);
			this->testString(uri->getSiteKeyString(), "sqrlid.com");
			this->testString(uri->getChallenge(), "sqrl://sqrlid.com/login?nut=blah&sfn=U1FSTGlk");
			this->testString(uri->getUrl(), "https://sqrlid.com/login?nut=blah&sfn=U1FSTGlk");
			this->testString(uri->getPrefix(), "https://sqrlid.com");
			this->testString(uri->getSFN(), "SQRLid");
			uri->release();
			delete (NullClient*)NullClient::getClient();
		}
		
		TEST_METHOD(Uri3)
		{
			new NullClient();
			SqrlUri *uri = SqrlUri::parse("sqrl://sqrlid.com:8080/login?sfn=U1FSTGlk&nut=blah");
			Assert::IsNotNull( uri );
			Assert::IsTrue(uri->getScheme() == SQRL_SCHEME_SQRL);
			this->testString(uri->getSiteKeyString(), "sqrlid.com");
			this->testString(uri->getChallenge(), "sqrl://sqrlid.com:8080/login?sfn=U1FSTGlk&nut=blah");
			this->testString(uri->getUrl(), "https://sqrlid.com:8080/login?sfn=U1FSTGlk&nut=blah");
			this->testString(uri->getPrefix(), "https://sqrlid.com:8080");
			this->testString(uri->getSFN(), "SQRLid");
			uri->release();
			delete (NullClient*)NullClient::getClient();
		}
		
		TEST_METHOD(FileUri)
		{
			new NullClient();
			SqrlUri *uri = SqrlUri::parse("file://test1.sqrl");
			Assert::IsNotNull( uri );
			Assert::IsTrue(uri->getScheme() == SQRL_SCHEME_FILE);
			Assert::IsTrue(uri->getSiteKeyStringLength() == 0);
			this->testString(uri->getUrl(), "file://test1.sqrl");
			this->testString(uri->getChallenge(), "test1.sqrl");
			this->testString(uri->getPrefix(), NULL);
			this->testString(uri->getSFN(), NULL);
			uri->release();
			delete (NullClient*)NullClient::getClient();
		}
		
		TEST_METHOD(SQRLUriWithoutSFN)
		{
			new NullClient();
			SqrlUri *uri = SqrlUri::parse("sqrl://sqrlid.com:8080/login?nut=blah");
			Assert::IsNull( uri );
			delete (NullClient*)NullClient::getClient();
		}
		
		TEST_METHOD(InvalidSQRLUrl)
		{
			new NullClient();
			SqrlUri *uri = SqrlUri::parse("http://google.com");
			Assert::IsNull( uri );
			delete (NullClient*)NullClient::getClient();
		}

		TEST_CLASS_CLEANUP(StopSqrl)
		{
		}
	};

}