import unittest
from unittest.mock import Mock, patch
import gdocs

class ConnectionChecks(unittest.TestCase):
    def test_scope(self):
        self.assertEqual(gdocs.SCOPES, ["https://www.googleapis.com/auth/documents"])

    def test_missing_login_fails_without_browser(self):
        with patch("gdocs.os.path.exists", return_value=False), patch("gdocs.InstalledAppFlow") as flow:
            with self.assertRaisesRegex(RuntimeError, "gdocs.cmd auth"):
                gdocs.get_credentials()
            flow.from_client_secrets_file.assert_not_called()

    def test_expired_token_refreshes_and_saves(self):
        creds = Mock(valid=False, refresh_token="test-placeholder")
        with patch("gdocs.os.path.exists", return_value=True), patch("gdocs.Credentials.from_authorized_user_file", return_value=creds), patch("gdocs.save_credentials") as save:
            self.assertIs(gdocs.get_credentials(), creds)
            creds.refresh.assert_called_once()
            save.assert_called_once_with(creds)

    def test_missing_tab_does_not_read_another_tab(self):
        doc = {"tabs": [{"tabProperties": {"tabId": "existing"}, "documentTab": {"body": {"content": []}}}]}
        with self.assertRaises(ValueError):
            gdocs.get_tab_content(doc, "missing")

if __name__ == "__main__":
    unittest.main()