NULL =

SUBDIRS = icons

gsettings_file = org.ayatana.indicator.notifications.gschema.xml
gsettings_SCHEMAS = $(gsettings_file)

@INTLTOOL_XML_NOMERGE_RULE@

$(gsettings_file).in: $(gsettings_file).in.in
	sed \
	  -e "s|\@GETTEXT_PACKAGE\@|$(GETTEXT_PACKAGE)|" \
	  $< > $@

@GSETTINGS_RULES@

#if defined(HAVE_SYSTEMD)
systemdservice_file = ayatana-indicator-notifications.service
$(systemdservice_file): $(systemdservice_file).in
	$(AM_V_GEN) sed -e "s|\@pkglibexecdir\@|$(pkglibexecdir)|" $< > $@

systemddir = $(SYSTEMD_USERDIR)
systemd_DATA = $(systemdservice_file)
#endif

EXTRA_DIST = \
	$(gsettings_file).in.in \
	$(systemdservice_file).in \
	$(NULL)

CLEANFILES = \
	$(gsettings_file) \
	$(gsettings_file).in \
	*.gschema.valid \
	$(systemdservice_file) \
	$(NULL)
