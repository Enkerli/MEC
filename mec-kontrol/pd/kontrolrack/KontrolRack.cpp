#include "KontrolRack.h"

#include <dirent.h>
#include <sys/stat.h>

#include <clocale>

/*****
represents the device interface, of which there is always one
******/

static t_class *KontrolRack_class;

class PdCallback : public Kontrol::KontrolCallback {
public:
    PdCallback(t_KontrolRack *x) : x_(x) {
        ;
    }

    //Kontrol::KontrolCallback
    void rack(Kontrol::ChangeSource, const Kontrol::Rack &) override { ; }

    void module(Kontrol::ChangeSource, const Kontrol::Rack &, const Kontrol::Module &) override { ; }

    void page(Kontrol::ChangeSource, const Kontrol::Rack &, const Kontrol::Module &,
              const Kontrol::Page &) override { ; }

    void param(Kontrol::ChangeSource, const Kontrol::Rack &, const Kontrol::Module &,
               const Kontrol::Parameter &) override { ; }

    void changed(Kontrol::ChangeSource, const Kontrol::Rack &, const Kontrol::Module &,
                 const Kontrol::Parameter &) override;

    void resource(Kontrol::ChangeSource, const Kontrol::Rack &,
                  const std::string &, const std::string &) override { ; }


    void loadModule(Kontrol::ChangeSource, const Kontrol::Rack &,
                    const Kontrol::EntityId &, const std::string &) override;
private:
    t_KontrolRack *x_;
};

// puredata methods implementation - start

static const unsigned OSC_POLL_FREQUENCY = 1; // 10ms
static const unsigned DEVICE_POLL_FREQUENCY = 1; // 10ms
static const unsigned OSC_PING_FREQUENCY_SEC = 5;
static const unsigned OSC_PING_FREQUENCY = (OSC_PING_FREQUENCY_SEC * (1000 / 10)); // 5 seconds


// see https://github.com/pure-data/pure-data/blob/master/src/x_time.c
static const float TICK_MS = 1.0f * 10.0f; // 10.ms


/// main PD methods
void KontrolRack_tick(t_KontrolRack *x) {
    x->pollCount_++;
    if (x->osc_receiver_ && x->pollCount_ % OSC_POLL_FREQUENCY == 0) {
        x->osc_receiver_->poll();
    }

    if (x->device_ && x->pollCount_ % DEVICE_POLL_FREQUENCY == 0) {
        x->device_->poll();
    }

    if (x->osc_broadcaster_ && x->osc_receiver_
        && x->pollCount_ % OSC_PING_FREQUENCY == 0) {
        x->osc_broadcaster_->sendPing(x->osc_receiver_->port());
    }

    clock_delay(x->x_clock, 1);
}


void KontrolRack_free(t_KontrolRack *x) {
    clock_free(x->x_clock);
    x->model_->clearCallbacks();
    if (x->osc_receiver_) x->osc_receiver_->stop();
    x->osc_receiver_.reset();
    x->device_.reset();
    // Kontrol::ParameterModel::free();
}

void *KontrolRack_new(t_floatarg serverport, t_floatarg clientport) {
    t_KontrolRack *x = (t_KontrolRack *) pd_new(KontrolRack_class);


    x->osc_receiver_ = nullptr;

    x->pollCount_ = 0;
    x->model_ = Kontrol::KontrolModel::model();

    x->model_->createLocalRack((unsigned int) clientport);

    x->device_ = std::make_shared<Organelle>();
    x->device_->init();

    x->device_->currentRack(x->model_->localRackId());

    x->model_->addCallback("pd.send", std::make_shared<PdCallback>(x));
    x->model_->addCallback("pd.dev", x->device_);

    if (clientport) KontrolRack_listen(x, clientport);
    KontrolRack_connect(x, serverport);

    x->x_clock = clock_new(x, (t_method) KontrolRack_tick);
    clock_setunit(x->x_clock, TICK_MS, 0);
    clock_delay(x->x_clock, 1);

    KontrolRack_loadresources(x);

    return (void *) x;
}

void KontrolRack_setup(void) {
    KontrolRack_class = class_new(gensym("KontrolRack"),
                                  (t_newmethod) KontrolRack_new,
                                  (t_method) KontrolRack_free,
                                  sizeof(t_KontrolRack),
                                  CLASS_DEFAULT,
                                  A_DEFFLOAT,
                                  A_DEFFLOAT,
                                  A_NULL);

    class_addmethod(KontrolRack_class,
                    (t_method) KontrolRack_listen, gensym("listen"),
                    A_DEFFLOAT, A_NULL);
    class_addmethod(KontrolRack_class,
                    (t_method) KontrolRack_connect, gensym("connect"),
                    A_DEFFLOAT, A_NULL);

    class_addmethod(KontrolRack_class,
                    (t_method) KontrolRack_knob1Raw, gensym("knob1Raw"),
                    A_DEFFLOAT, A_NULL);
    class_addmethod(KontrolRack_class,
                    (t_method) KontrolRack_knob2Raw, gensym("knob2Raw"),
                    A_DEFFLOAT, A_NULL);
    class_addmethod(KontrolRack_class,
                    (t_method) KontrolRack_knob3Raw, gensym("knob3Raw"),
                    A_DEFFLOAT, A_NULL);
    class_addmethod(KontrolRack_class,
                    (t_method) KontrolRack_knob4Raw, gensym("knob4Raw"),
                    A_DEFFLOAT, A_NULL);

    class_addmethod(KontrolRack_class,
                    (t_method) KontrolRack_enc, gensym("enc"),
                    A_DEFFLOAT, A_NULL);
    class_addmethod(KontrolRack_class,
                    (t_method) KontrolRack_encbut, gensym("encbut"),
                    A_DEFFLOAT, A_NULL);

    class_addmethod(KontrolRack_class,
                    (t_method) KontrolRack_midiCC, gensym("midiCC"),
                    A_DEFFLOAT, A_DEFFLOAT, A_NULL);

    class_addmethod(KontrolRack_class,
                    (t_method) KontrolRack_key, gensym("key"),
                    A_DEFFLOAT, A_DEFFLOAT, A_NULL);

    // class_addmethod(KontrolRack_class,
    //                 (t_method) KontrolRack_vol, gensym("vol"),
    //                 A_DEFFLOAT, A_NULL);
    // class_addmethod(KontrolRack_class,
    //                 (t_method) KontrolRack_expRaw, gensym("expRaw"),
    //                 A_DEFFLOAT, A_NULL);
    // class_addmethod(KontrolRack_class,
    //                 (t_method) KontrolRack_fsRaw, gensym("fsRaw"),
    //                 A_DEFFLOAT, A_NULL);



    class_addmethod(KontrolRack_class,
                    (t_method) KontrolRack_loadsettings, gensym("loadsettings"),
                    A_DEFSYMBOL, A_NULL);
    class_addmethod(KontrolRack_class,
                    (t_method) KontrolRack_savesettings, gensym("savesettings"),
                    A_DEFSYMBOL, A_NULL);
    class_addmethod(KontrolRack_class,
                    (t_method) KontrolRack_loadpreset, gensym("loadpreset"),
                    A_DEFSYMBOL, A_NULL);
    class_addmethod(KontrolRack_class,
                    (t_method) KontrolRack_updatepreset, gensym("updatepreset"),
                    A_DEFSYMBOL, A_NULL);

    class_addmethod(KontrolRack_class,
                    (t_method) KontrolRack_loadmodule, gensym("loadmodule"),
                    A_DEFSYMBOL, A_DEFSYMBOL, A_NULL);

}

void KontrolRack_loadmodule(t_KontrolRack *x, t_symbol *modId, t_symbol *mod) {
    if (modId == nullptr && modId->s_name == nullptr && strlen(modId->s_name) == 0) {
        post("loadmodule failed , invalid moduleId");
        return;
    }
    if (mod == nullptr && mod->s_name == nullptr && strlen(mod->s_name) == 0) {
        post("loadmodule failed , invalid module for moduleId %s", modId->s_name);
        return;
    }

    std::string pdObject = std::string("pd-") + std::string(modId->s_name);
    std::string pdModule = std::string(mod->s_name) + "/module";

    t_pd *sendObj = gensym(pdObject.c_str())->s_thing;
    if (!sendObj) {
        post("loadmodule: unable to find %s", pdObject.c_str());
        return;
    }

    post("loadmodule: loading %s into %s", mod->s_name, modId->s_name);
    {
        // clear the object
        t_atom args[1];
        SETSYMBOL(&args[0], gensym("clear"));
        pd_forwardmess(sendObj, 1, args);
    }

    {
        // create module
        t_symbol *modSym = gensym(pdModule.c_str());
        t_atom args[5];
        SETSYMBOL(&args[0], gensym("obj"));
        SETFLOAT(&args[1], 100);
        SETFLOAT(&args[2], 100);
        SETSYMBOL(&args[3], modSym);
        SETSYMBOL(&args[4], modId);
        pd_forwardmess(sendObj, 5, args);
    }

    auto rack = Kontrol::KontrolModel::model()->getLocalRack();
    auto module = Kontrol::KontrolModel::model()->getModule(rack, modId->s_name);
    if (module == nullptr) {
        post("unable to initialise moduled %s %s", modId->s_name, mod->s_name);
    }

    {
        //load defintions
        std::string sendsym = std::string("loaddefs-") + modId->s_name;
        t_pd *sendobj = gensym(sendsym.c_str())->s_thing;
        if (sendobj != nullptr) {
            std::string file = std::string(mod->s_name) + "/" + module->type() + "-module.json";
            t_symbol *fileSym = gensym(file.c_str());
            t_atom args[1];
            SETSYMBOL(&args[0], fileSym);
            pd_forwardmess(sendobj, 1, args);
        }

    }


    {
        // loadbang
        t_atom args[5];
        SETSYMBOL(&args[0], gensym("obj"));
        SETFLOAT(&args[1], 400);
        SETFLOAT(&args[2], 0);
        SETSYMBOL(&args[3], gensym("r"));
        SETSYMBOL(&args[4], gensym("rackloaded"));
        pd_forwardmess(sendObj, 5, args);

    }
    {
        std::string file = std::string(mod->s_name) + "/" + module->type() + "-module.json";
        t_symbol *fileSym = gensym(file.c_str());
        t_atom args[4];
        SETSYMBOL(&args[0], gensym("msg"));
        SETFLOAT(&args[1], 400);
        SETFLOAT(&args[2], 50);
        SETSYMBOL(&args[3], fileSym);
        pd_forwardmess(sendObj, 4, args);
    }

    {
        std::string sym = std::string("loaddefs-") + modId->s_name;
        t_atom args[5];
        SETSYMBOL(&args[0], gensym("obj"));
        SETFLOAT(&args[1], 400);
        SETFLOAT(&args[2], 100);
        SETSYMBOL(&args[3], gensym("send"));
        SETSYMBOL(&args[4], gensym(sym.c_str()));
        pd_forwardmess(sendObj, 5, args);
    }

    {
        t_atom args[5];
        SETSYMBOL(&args[0], gensym("connect"));
        SETFLOAT(&args[1], 1);
        SETFLOAT(&args[2], 0);
        SETFLOAT(&args[3], 2);
        SETFLOAT(&args[4], 0);
        pd_forwardmess(sendObj, 5, args);
    }
    {
        t_atom args[5];
        SETSYMBOL(&args[0], gensym("connect"));
        SETFLOAT(&args[1], 2);
        SETFLOAT(&args[2], 0);
        SETFLOAT(&args[3], 3);
        SETFLOAT(&args[4], 0);
        pd_forwardmess(sendObj, 5, args);
    }

    {
        // send a fake loadbang after its loaded
        std::string sendsym = std::string("loadbang-") + modId->s_name;
        t_pd *sendobj = gensym(sendsym.c_str())->s_thing;
        if (sendobj != nullptr) {
            t_atom args[1];
            SETFLOAT(&args[0], 1);
            pd_forwardmess(sendobj, 1, args);
        } else {
            post("loadbang missing for %s", sendsym.c_str());
        }
    }
}


void KontrolRack_connect(t_KontrolRack *x, t_floatarg f) {
    if (f > 0) {
        std::string host = "127.0.0.1";
        unsigned port = (unsigned) f;
        std::string srcId = host + ":" + std::to_string(port);
        std::string id = "pd.osc:" + srcId;
        x->model_->removeCallback(id);
        auto p = std::make_shared<Kontrol::OSCBroadcaster>(
                Kontrol::ChangeSource(Kontrol::ChangeSource::REMOTE, srcId),
                OSC_PING_FREQUENCY_SEC,
                false);
        if (p->connect(host, port)) {
            post("client connected %s", id.c_str());
            x->model_->addCallback(id, p);
            x->osc_broadcaster_ = p;
            if (x->osc_receiver_) {
                x->osc_broadcaster_->sendPing(x->osc_receiver_->port());
            }
        }
    }
}

void KontrolRack_listen(t_KontrolRack *x, t_floatarg f) {
    if (f > 0) {
        auto p = std::make_shared<Kontrol::OSCReceiver>(x->model_);
        if (p->listen((unsigned) f)) {
            x->osc_receiver_ = p;
        }
    } else {
        x->osc_receiver_.reset();
    }
}

void KontrolRack_enc(t_KontrolRack *x, t_floatarg f) {
    if (x->device_) x->device_->changeEncoder(0, f);
}

void KontrolRack_encbut(t_KontrolRack *x, t_floatarg f) {
    if (x->device_) x->device_->encoderButton(0, (bool) f);
}

void KontrolRack_knob1Raw(t_KontrolRack *x, t_floatarg f) {
    if (x->device_) x->device_->changePot(0, f);
}

void KontrolRack_knob2Raw(t_KontrolRack *x, t_floatarg f) {
    if (x->device_) x->device_->changePot(1, f);
}

void KontrolRack_knob3Raw(t_KontrolRack *x, t_floatarg f) {
    if (x->device_) x->device_->changePot(2, f);
}

void KontrolRack_knob4Raw(t_KontrolRack *x, t_floatarg f) {
    if (x->device_) x->device_->changePot(3, f);
}

void KontrolRack_midiCC(t_KontrolRack *x, t_floatarg cc, t_floatarg value) {
    if (x->device_) x->device_->midiCC((unsigned) cc, (unsigned) value);
}

void KontrolRack_key(t_KontrolRack *x, t_floatarg key, t_floatarg value) {
    if (x->device_) x->device_->keyPress((unsigned) key, (unsigned) value);
}


void KontrolRack_loadsettings(t_KontrolRack *x, t_symbol *settings) {
    if (settings != nullptr && settings->s_name != nullptr && strlen(settings->s_name) > 0) {
        auto rack = Kontrol::KontrolModel::model()->getLocalRack();
        if (rack) {
            std::string settingsId = settings->s_name;
            post("loading settings : %s", settings->s_name);
            rack->loadSettings(settingsId + "-rack.json");
            rack->dumpSettings();
        } else {
            post("No local rack found");
        };
    }
}

void KontrolRack_savesettings(t_KontrolRack *x, t_symbol *settings) {
    if (settings != nullptr && settings->s_name != nullptr && strlen(settings->s_name) > 0) {
        auto rack = Kontrol::KontrolModel::model()->getLocalRack();
        if (rack) {
            std::string settingsId = settings->s_name;
            post("saving settings : %s", settings->s_name);
            rack->saveSettings(settingsId + "-rack.json");
        } else {
            post("No local rack found");
        };
    }
}


void KontrolRack_loadpreset(t_KontrolRack *x, t_symbol *preset) {
    if (preset != nullptr && preset->s_name != nullptr && strlen(preset->s_name) > 0) {
        auto rack = Kontrol::KontrolModel::model()->getLocalRack();
        if (rack) {
            std::string presetId = preset->s_name;
            post("loading preset : %s", preset->s_name);
            rack->applyPreset(presetId);
            rack->dumpCurrentValues();
        }
    } else {
        post("No local rack found");
    }
}

void KontrolRack_updatepreset(t_KontrolRack *x, t_symbol *preset) {
    if (preset != nullptr && preset->s_name != nullptr && strlen(preset->s_name) > 0) {
        auto rack = Kontrol::KontrolModel::model()->getLocalRack();
        if (rack) {
            std::string presetId = preset->s_name;
            post("update preset : %s", preset->s_name);
            rack->updatePreset(presetId);
        }
    } else {
        post("No local rack found");
    }
}


void KontrolRack_loadresources(t_KontrolRack *x) {
    post("KontrolRack::loading resources");

    auto rack = Kontrol::KontrolModel::model()->getLocalRack();
    if (rack == nullptr) return;

    struct dirent **namelist;
    struct stat st;
    static const std::string MODULE_DIR = "modules";
    std::setlocale(LC_ALL, "en_US.UTF-8");
    int n = scandir(MODULE_DIR.c_str(), &namelist, NULL, alphasort);
    if (n > 0) {
        for (int i = 0; i < n; i++) {
            if (namelist[i]->d_type == DT_DIR &&
                strcmp(namelist[i]->d_name, "..") != 0
                && strcmp(namelist[i]->d_name, ".") != 0) {

                std::string module = MODULE_DIR + "/" + std::string(namelist[i]->d_name) + "/module.pd";
                int fs = stat(module.c_str(), &st);
                if (fs == 0) {
                    rack->addResource("module", namelist[i]->d_name);
                    post("KontrolRack::module found: %s", namelist[i]->d_name);
                }
            }
        }
    }
}


//-----------------------
void PdCallback::changed(Kontrol::ChangeSource,
                         const Kontrol::Rack &rack,
                         const Kontrol::Module &module,
                         const Kontrol::Parameter &param) {

    std::string sendsym = param.id() + "-" + module.id();
    t_pd *sendobj = gensym(sendsym.c_str())->s_thing;


    if (!sendobj) {
        post("send to %s failed", param.id().c_str());
        return;
    }

    t_atom a;
    switch (param.current().type()) {
        case Kontrol::ParamValue::T_Float : {
            SETFLOAT(&a, param.current().floatValue());
            break;
        }
        case Kontrol::ParamValue::T_String:
        default: {
            SETSYMBOL(&a, gensym(param.current().stringValue().c_str()));
            break;
        }
    }
    pd_forwardmess(sendobj, 1, &a);
}

void PdCallback::loadModule(Kontrol::ChangeSource, const Kontrol::Rack &r,
                            const Kontrol::EntityId &mId, const std::string &mType) {
    auto rack = Kontrol::KontrolModel::model()->getLocalRack();
    if (rack && rack->id() == r.id()) {
        // load the module
        std::string mod = std::string("modules/") + mType;
        t_symbol *modId = gensym(mId.c_str());
        t_symbol *modType = gensym(mod.c_str());
        KontrolRack_loadmodule(x_, modId, modType);
    } else {
        post("No local rack found");
    }
}



// puredata methods implementation - end
