static void
entry_key_pressed(GntWidget *w, FinchConv *ggconv)
{
	const char *text = gnt_entry_get_text(GNT_ENTRY(ggconv->entry));
	if (*text == '/' && *(text + 1) != '/')
	{
		PurpleConversation *conv = ggconv->active_conv;
		PurpleCmdStatus status;
		const char *cmdline = text + 1;
		char *error = NULL, *escape;

		escape = g_markup_escape_text(cmdline, -1);
		status = purple_cmd_do_command(conv, cmdline, escape, &error);
		g_free(escape);

		switch (status)
		{
			case PURPLE_CMD_STATUS_OK:
				break;
			case PURPLE_CMD_STATUS_NOT_FOUND:
				purple_conversation_write(conv, "", _("No such command."),
						PURPLE_MESSAGE_NO_LOG, time(NULL));
				break;
			case PURPLE_CMD_STATUS_WRONG_ARGS:
				purple_conversation_write(conv, "", _("Syntax Error:  You typed the wrong number of arguments "
							"to that command."),
						PURPLE_MESSAGE_NO_LOG, time(NULL));
				break;
			case PURPLE_CMD_STATUS_FAILED:
				purple_conversation_write(conv, "", error ? error : _("Your command failed for an unknown reason."),
						PURPLE_MESSAGE_NO_LOG, time(NULL));
				break;
			case PURPLE_CMD_STATUS_WRONG_TYPE:
				if(purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_IM)
					purple_conversation_write(conv, "", _("That command only works in chats, not IMs."),
							PURPLE_MESSAGE_NO_LOG, time(NULL));
				else
					purple_conversation_write(conv, "", _("That command only works in IMs, not chats."),
							PURPLE_MESSAGE_NO_LOG, time(NULL));
				break;
			case PURPLE_CMD_STATUS_WRONG_PRPL:
				purple_conversation_write(conv, "", _("That command doesn't work on this protocol."),
						PURPLE_MESSAGE_NO_LOG, time(NULL));
				break;
		}
		g_free(error);
	}
	else if (!purple_account_is_connected(purple_conversation_get_account(ggconv->active_conv)))
	{
		purple_conversation_write(ggconv->active_conv, "", _("Message was not sent, because you are not signed on."),
				PURPLE_MESSAGE_ERROR | PURPLE_MESSAGE_NO_LOG, time(NULL));
	}
	else
	{
		char *escape = purple_markup_escape_text((*text == '/' ? text + 1 : text), -1);
		switch (purple_conversation_get_type(ggconv->active_conv))
		{
			case PURPLE_CONV_TYPE_IM:
				purple_conv_im_send_with_flags(PURPLE_CONV_IM(ggconv->active_conv), escape, PURPLE_MESSAGE_SEND);
				break;
			case PURPLE_CONV_TYPE_CHAT:
				purple_conv_chat_send(PURPLE_CONV_CHAT(ggconv->active_conv), escape);
				break;
			default:
				g_free(escape);
				g_return_if_reached();
		}
		g_free(escape);
		purple_idle_touch();
	}
	gnt_entry_add_to_history(GNT_ENTRY(ggconv->entry), text);
	gnt_entry_clear(GNT_ENTRY(ggconv->entry));
}
