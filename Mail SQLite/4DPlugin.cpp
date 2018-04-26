/* --------------------------------------------------------------------------------
 #
 #	4DPlugin.cpp
 #	source generated by 4D Plugin Wizard
 #	Project : Mail (SQLite)
 #	author : miyako
 #	4/25/18
 #
 # --------------------------------------------------------------------------------*/


#include "4DPluginAPI.h"
#include "4DPlugin.h"

#pragma mark JSON

void json_stringify(JSONNODE *json, C_TEXT &t)
{
	//json_char *json_string = json_write_formatted(json);
	json_char *json_string = json_write(json);
	std::wstring wstr = std::wstring(json_string);
	
#if VERSIONWIN
	t.setUTF16String((const PA_Unichar *)wstr.c_str(), (uint32_t)wstr.length());
#else
	uint32_t dataSize = (uint32_t)((wstr.length() * sizeof(wchar_t))+ sizeof(PA_Unichar));
	std::vector<char> buf(dataSize);
	uint32_t len = PA_ConvertCharsetToCharset((char *)wstr.c_str(),
																						(PA_long32)(wstr.length() * sizeof(wchar_t)),
																						eVTC_UTF_32,
																						(char *)&buf[0],
																						dataSize,
																						eVTC_UTF_16);
	t.setUTF16String((const PA_Unichar *)&buf[0], len);
#endif
	json_free(json_string);
}

void json_set_s(JSONNODE *n, const wchar_t *name, const unsigned char *value)
{
	if(n)
	{
		C_TEXT t;
		CUTF8String u8;
		
		if(value)
			u8 = (const uint8_t *)value;
		
		t.setUTF8String(&u8);
		
		std::wstring u32;
#if VERSIONWIN
		u32 = std::wstring((wchar_t *)value.getUTF16StringPtr());
#else
		
		uint32_t dataSize = (t.getUTF16Length() * sizeof(wchar_t))+ sizeof(wchar_t);
		std::vector<char> buf(dataSize);
		
		PA_ConvertCharsetToCharset((char *)t.getUTF16StringPtr(),
															 t.getUTF16Length() * sizeof(PA_Unichar),
															 eVTC_UTF_16,
															 (char *)&buf[0],
															 dataSize,
															 eVTC_UTF_32);
		
		u32 = std::wstring((wchar_t *)&buf[0]);
#endif
		json_push_back(n, json_new_a(name, u32.c_str()));
	}
}

void json_set_i(JSONNODE *n, const wchar_t *name, int value)
{
	if(n)
	{
		json_push_back(n, json_new_i(name, value));
	}
}

#pragma mark -
void PluginMain(PA_long32 selector, PA_PluginParameters params)
{
	try
	{
		PA_long32 pProcNum = selector;
		sLONG_PTR *pResult = (sLONG_PTR *)params->fResult;
		PackagePtr pParams = (PackagePtr)params->fParameters;

		CommandDispatcher(pProcNum, pResult, pParams); 
	}
	catch(...)
	{

	}
}

void CommandDispatcher (PA_long32 pProcNum, sLONG_PTR *pResult, PackagePtr pParams)
{
	switch(pProcNum)
	{
// --- Mail

		case 1 :
			Mail_Get_message_properties(pResult, pParams);
			break;

	}
}

// ------------------------------------- Mail -------------------------------------

NSURL *get_library_url()
{
	NSArray *URLs = [[NSFileManager defaultManager]
									 URLsForDirectory:NSLibraryDirectory
									 inDomains:NSUserDomainMask];
	
	return (URLs && [URLs count]) ? [URLs objectAtIndex:0] : nil;
}

#include <errno.h>
#include <sys/sysctl.h>

NSString *get_sqlite3_path_for_mail_for_current_version()
{
	NSString *path = @"/V6/MailData/Envelope Index";
	
	char str[64];
	size_t size = sizeof(str);
	
	if(!sysctlbyname("kern.osrelease", str, &size, NULL, 0))
	{
		@autoreleasepool
		{
			NSString *version = [NSString stringWithFormat:@"%s", str];
			
			if([version hasPrefix:@"17."])
			{
				path = @"/V5/MailData/Envelope Index";
			}else if([version hasPrefix:@"16."])
			{
				path = @"/V4/MailData/Envelope Index";
			}else if([version hasPrefix:@"15."])
			{
				path = @"/V3/MailData/Envelope Index";
			}
			else if( ([version hasPrefix:@"14."])
							|([version hasPrefix:@"13."])
							|([version hasPrefix:@"12."])
							|([version hasPrefix:@"11."]))
			{
				path = @"/V2/MailData/Envelope Index";
			}
			else
			{
				path = @"/Envelope Index";
			}
			
		}/* @autoreleasepool */
		
	}
	
	return path;
}

const char *get_sqlite3_path_for_mail()
{
	NSURL *library_url = get_library_url();
	
	if(!library_url)
		return 0;

	return [[[[library_url URLByAppendingPathComponent:@"Mail"] path]
					 stringByAppendingString:get_sqlite3_path_for_mail_for_current_version()] UTF8String];
}

#pragma mark SQL

namespace SQL
{
	NSString *toString(const unsigned char *value)
	{
		return value ? [NSString stringWithUTF8String:(const char *)value] : @"";
	}
}

#include "sqlite3.h"

void Mail_Get_message_properties(sLONG_PTR *pResult, PackagePtr pParams)
{
	ARRAY_LONGINT Param1;
	C_TEXT returnValue;

	JSONNODE *json = json_new(JSON_NODE);
	
	const char *path = get_sqlite3_path_for_mail();
	
	if(path)
	{
		sqlite3 *ds = NULL;
		
		if(SQLITE_OK == sqlite3_open(path, &ds))
		{
			Param1.fromParamAtIndex(pParams, 1);
			NSUInteger capacity = Param1.getSize();
			NSMutableArray *values = [[NSMutableArray alloc]initWithCapacity:capacity];

			@autoreleasepool
			{
				
				const char *csv0 = "select value from properties where key == 'UUID' limit 1;";
				sqlite3_stmt *sql0 = NULL;
				
				if(SQLITE_OK == sqlite3_prepare_v2(ds, csv0, strlen(csv0), &sql0, NULL))
				{
					if(SQLITE_ROW == sqlite3_step(sql0))
					{
						json_set_s(json, L"UUID", sqlite3_column_text(sql0, 0));
					}
				}
				
				for(unsigned int i = 1;i < capacity;++i)
				{
					[values addObject:[NSNumber numberWithInteger:Param1.getIntValueAtIndex(i)]];
				}
				
				const char *csv1 = [[NSString stringWithFormat:
														@"select \n\
messages.ROWID, \n\
mailboxes.url, \n\
addresses.address, \n\
addresses.comment, \n\
subjects.subject, \n\
messages.snippet, \n\
messages.sender, \n\
messages.size, \n\
messages.color, \n\
messages.type, \n\
messages.flagged, \n\
messages.flags, \n\
messages.date_sent, \n\
messages.date_received, \n\
messages.date_created \n\
from \n\
addresses,messages,subjects,mailboxes \n\
where \n\
messages.ROWID IN (%@) \n\
and addresses.ROWID == messages.sender \n\
and subjects.ROWID == messages.subject \n\
and mailboxes.ROWID == messages.mailbox \n\
;",
														[values componentsJoinedByString:@","]] UTF8String];
			
				/* messages: ROWID|message_id|document_id|in_reply_to|remote_id|sender|subject_prefix|subject|date_sent|date_received|date_created|date_last_viewed|mailbox|remote_mailbox|flags|read|flagged|size|color|type|conversation_id|snippet|fuzzy_ancestor|automated_conversation|root_status|conversation_position|deleted */
				sqlite3_stmt *sql1 = NULL;
				
				if(SQLITE_OK == sqlite3_prepare_v2(ds, csv1, strlen(csv1), &sql1, NULL))
				{
					JSONNODE *json_messages = json_new(JSON_ARRAY);
					
					while(SQLITE_ROW == sqlite3_step(sql1))
					{
						
						int m = sqlite3_column_int(sql1, 0);
						
						JSONNODE *json_message = json_new(JSON_NODE);
						json_set_i(json_message, L"ID", m);
						json_set_s(json_message, L"mailbox", sqlite3_column_text(sql1, 1));
						json_set_s(json_message, L"subject", sqlite3_column_text(sql1, 4));
						json_set_s(json_message, L"snippet", sqlite3_column_text(sql1, 5));

						json_set_i(json_message, L"size", sqlite3_column_int(sql1, 7));
						json_set_i(json_message, L"color", sqlite3_column_int(sql1, 8));
						json_set_i(json_message, L"type", sqlite3_column_int(sql1, 9));
						json_set_i(json_message, L"flagged", sqlite3_column_int(sql1, 10));

						json_set_i(json_message, L"flags", sqlite3_column_int(sql1, 11));
						json_set_i(json_message, L"date_sent", sqlite3_column_int(sql1, 12));
						json_set_i(json_message, L"date_received", sqlite3_column_int(sql1, 13));
						json_set_i(json_message, L"date_created", sqlite3_column_int(sql1, 14));
						
						JSONNODE *json_sender = json_new(JSON_NODE);
						json_set_i(json_sender, L"ID", sqlite3_column_int(sql1, 6));
						json_set_s(json_sender, L"address", sqlite3_column_text(sql1, 2));
						json_set_s(json_sender, L"name", sqlite3_column_text(sql1, 3));
						json_set_name(json_sender, L"sender");
						json_push_back(json_message, json_sender);
						
						const char *csv2 = [[NSString stringWithFormat:
																 @"select \n\
attachments.ROWID, \n\
attachments.name \n\
from \n\
messages,attachments \n\
where \n\
messages.ROWID == %d \n\
and \n\
attachments.message_id == messages.ROWID \n\
;",
m] UTF8String];

						/* attachments: ROWID|message_id|name */
						sqlite3_stmt *sql2 = NULL;
						if(SQLITE_OK == sqlite3_prepare_v2(ds, csv2, strlen(csv2), &sql2, NULL))
						{
							JSONNODE *json_attachments = json_new(JSON_ARRAY);
							while(SQLITE_ROW == sqlite3_step(sql2))
							{
								JSONNODE *json_attachment = json_new(JSON_NODE);
								json_set_i(json_attachment, L"ID", sqlite3_column_int(sql2, 0));
								json_set_s(json_attachment, L"name", sqlite3_column_text(sql2, 1));
								json_push_back(json_attachments, json_attachment);
							}
							json_set_name(json_attachments, L"attachments");
							json_push_back(json_message, json_attachments);
						}
						
						const char *csv3 = [[NSString stringWithFormat:
																 @"select \n\
recipients.ROWID, \n\
recipients.type, \n\
recipients.position, \n\
addresses.address, \n\
addresses.comment \n\
from \n\
recipients,addresses,messages \n\
where \n\
messages.ROWID == %d \n\
and \n\
recipients.message_id == messages.ROWID \n\
and \n\
addresses.ROWID == recipients.address_id \n\
;",
m] UTF8String];
						
						/* recipients: ROWID|message_id|type|address_id|position */
						sqlite3_stmt *sql3 = NULL;
						if(SQLITE_OK == sqlite3_prepare_v2(ds, csv3, strlen(csv3), &sql3, NULL))
						{
							JSONNODE *json_recipients = json_new(JSON_ARRAY);
							while(SQLITE_ROW == sqlite3_step(sql3))
							{
								JSONNODE *json_recipient = json_new(JSON_NODE);
								json_set_i(json_recipient, L"ID", sqlite3_column_int(sql3, 0));
//								json_set_i(json_recipient, L"type", sqlite3_column_int(sql3, 1));
//								json_set_i(json_recipient, L"position", sqlite3_column_int(sql3, 2));
								json_set_s(json_recipient, L"address", sqlite3_column_text(sql3, 3));
								json_set_s(json_recipient, L"name", sqlite3_column_text(sql3, 4));
								json_push_back(json_recipients, json_recipient);
							}
							json_set_name(json_recipients, L"recipients");
							json_push_back(json_message, json_recipients);
						}
						
						json_push_back(json_messages, json_message);
					}
					
					json_set_name(json_messages, L"messages");
					json_push_back(json, json_messages);
				}
				sqlite3_close(ds);
			}/* @autoreleasepool */
	
		}
		
	}
	
	json_stringify(json, returnValue);
	json_delete(json);
	
	returnValue.setReturn(pResult);
}

