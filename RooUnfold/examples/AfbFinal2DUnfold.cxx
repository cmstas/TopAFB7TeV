#include <iostream>
#include <fstream>
#include "AfbFinalUnfold.h"

#include "TRandom3.h"
#include "TH1D.h"
#include "TStyle.h"
#include "TFile.h"
#include "TTree.h"
#include "TCanvas.h"
#include "TMatrixD.h"
#include "TChain.h"
#include "TLegend.h"
#include "TColor.h"
#include "THStack.h"
#include "TCut.h"

#include "src/RooUnfold.h"
#include "src/RooUnfoldResponse.h"
#include "src/RooUnfoldBayes.h"
#include "src/RooUnfoldSvd.h"

#include "tdrstyle.C"

using std::cout;
using std::endl;


//==============================================================================
// Global definitions
//==============================================================================

//const Double_t _topScalingFactor=1.+(9824. - 10070.94)/9344.25;  //needs to be changed from preselection ratio to ratio for events with a ttbar solution

// 0=SVD, 1=TUnfold via RooUnfold, 2=TUnfold
int unfoldingType = 0;

TString Region = "";
Int_t kterm = 3;  //note we used 4 here for ICHEP
Double_t tau = 1E-4;
Int_t nVars = 8;
Int_t includeSys = 1;
Int_t checkErrors = 1;
bool draw_truth_before_pT_reweighting = true;


void AfbUnfoldExample(double scalettdil = 1., double scalettotr = 1., double scalewjets = 1., double scaleDY = 1., double scaletw = 1., double scaleVV = 1.)
{
    setTDRStyle();
    gStyle->SetOptFit();
    gStyle->SetOptStat(0);
    gStyle->SetOptTitle(0);
    cout.precision(3);

    TString summary_name = "summary_2Dunfolding";

    if (!(scalettotr == 1. && scalewjets == 1. && scaleDY == 1. && scaletw == 1. && scaleVV == 1.))  summary_name = Form("summary_2Dunfolding_%i_%i_%i_%i_%i", int(10.*scalettotr + 0.5), int(10.*scalewjets + 0.5), int(10.*scaleDY + 0.5), int(10.*scaletw + 0.5), int(10.*scaleVV + 0.5));

    TRandom3 *random = new TRandom3();
    random->SetSeed(5);

    ofstream myfile;
    myfile.open (summary_name + ".txt");
    cout.rdbuf(myfile.rdbuf());

    // OGU 130516: add second output txt file with format easier to be pasted into google docs
    ofstream second_output_file;
    second_output_file.open(summary_name + "_formated.txt");

    const int nBkg = 7;
    TString path = "../";
    TString bkgroot[nBkg] = {"ttotr.root", "wjets.root", "DYee.root", "DYmm.root", "DYtautau.root", "tw.root", "VV.root"};
    double bkgSF[nBkg] = {scalettotr, scalewjets, scaleDY, scaleDY, scaleDY, scaletw, scaleVV};


    Float_t observable, observable_gen, ttmass, ttmass_gen;
    Float_t observableMinus, observableMinus_gen;
    Double_t weight;
    Int_t Nsolns;

    for (Int_t iVar = 0; iVar < nVars; iVar++)
    {


        Initialize2DBinning(iVar);
        bool combineLepMinus = acceptanceName == "lepCosTheta" ? true : false;
        TH1D *hData = new TH1D ("Data_BkgSub", "Data with background subtracted",    nbins2D, xbins2D);
        TH1D *hBkg = new TH1D ("Background",  "Background",    nbins2D, xbins2D);
        TH1D *hData_unfolded = new TH1D ("Data_Unfold", "Data with background subtracted and unfolded", nbins2D, xbins2D);


        TH1D *hTrue = new TH1D ("true", "Truth",    nbins2D, xbins2D);
        TH1D *hMeas = new TH1D ("meas", "Measured", nbins2D, xbins2D);

        TH2D *hTrue_vs_Meas = new TH2D ("true_vs_meas", "True vs Measured", nbins2D, xbins2D, nbins2D, xbins2D);

        TH1D *hData_bkgSub;

        hData->Sumw2();
        hBkg->Sumw2();
        hData_unfolded->Sumw2();
        hTrue->Sumw2();
        hMeas->Sumw2();
        hTrue_vs_Meas->Sumw2();


        TMatrixD m_unfoldE(nbins2D, nbins2D);
        TMatrixD m_correctE(nbins2D, nbins2D);


        //  Now test with data and with BKG subtraction

        TChain *ch_bkg[nBkg];
        TChain *ch_top = new TChain("tree");

        TChain *ch_data = new TChain("tree");

        TString path = "../";

        ch_data->Add(path + "data.root");

        ch_top->Add(path + "ttdil.root");

        for (int iBkg = 0; iBkg < nBkg; ++iBkg)
        {
            ch_bkg[iBkg] = new TChain("tree");
            ch_bkg[iBkg]->Add(path + bkgroot[iBkg]);
        }


        ch_data->SetBranchAddress(observablename,    &observable);
        if ( combineLepMinus ) ch_data->SetBranchAddress("lepMinus_costheta_cms",    &observableMinus);
        ch_data->SetBranchAddress("weight", &weight);
        ch_data->SetBranchAddress("Nsolns", &Nsolns);
        ch_data->SetBranchAddress("tt_mass", &ttmass);

        for (Int_t i = 0; i < ch_data->GetEntries(); i++)
        {
            ch_data->GetEntry(i);
            if ( ttmass > 0 )
            {
                if (observablename == "lep_azimuthal_asymmetry2") observable = -cos(observable);
                fillUnderOverFlow(hData, sign(observable)*ttmass, weight, Nsolns);
                if (combineLepMinus)
                {
                    fillUnderOverFlow(hData, sign(observableMinus)*ttmass, weight, Nsolns);
                }
            }
        }


        for (int iBkg = 0; iBkg < nBkg; ++iBkg)
        {
            ch_bkg[iBkg]->SetBranchAddress(observablename,    &observable);
            if ( combineLepMinus ) ch_bkg[iBkg]->SetBranchAddress("lepMinus_costheta_cms",    &observableMinus);
            ch_bkg[iBkg]->SetBranchAddress("weight", &weight);
            ch_bkg[iBkg]->SetBranchAddress("Nsolns", &Nsolns);
            ch_bkg[iBkg]->SetBranchAddress("tt_mass", &ttmass);

            for (Int_t i = 0; i < ch_bkg[iBkg]->GetEntries(); i++)
            {
                ch_bkg[iBkg]->GetEntry(i);
                weight *= bkgSF[iBkg];
                if (observablename == "lep_azimuthal_asymmetry2") observable = -cos(observable);
                if ( ttmass > 0 )
                {
                    fillUnderOverFlow(hBkg, sign(observable)*ttmass, weight, Nsolns);
                    if (combineLepMinus)
                    {
                        fillUnderOverFlow(hBkg, sign(observableMinus)*ttmass, weight, Nsolns);
                    }
                }
            }
        }

        ch_top->SetBranchAddress(observablename,    &observable);
        ch_top->SetBranchAddress(observablename + "_gen", &observable_gen);
        if ( combineLepMinus ) ch_top->SetBranchAddress("lepMinus_costheta_cms",    &observableMinus);
        if ( combineLepMinus ) ch_top->SetBranchAddress("lepMinus_costheta_cms_gen",    &observableMinus_gen);
        ch_top->SetBranchAddress("weight", &weight);
        ch_top->SetBranchAddress("Nsolns", &Nsolns);
        ch_top->SetBranchAddress("tt_mass", &ttmass);
        ch_top->SetBranchAddress("tt_mass_gen", &ttmass_gen);

        for (Int_t i = 0; i < ch_top->GetEntries(); i++)
        {
            ch_top->GetEntry(i);
            weight *= scalettdil;
            if (observablename == "lep_azimuthal_asymmetry2") observable = -cos(observable);
            if (observablename == "lep_azimuthal_asymmetry2") observable_gen = -cos(observable_gen);
            if ( ttmass > 0 )
            {
                fillUnderOverFlow(hMeas, sign(observable)*ttmass, weight, Nsolns);
                fillUnderOverFlow(hTrue, sign(observable_gen)*ttmass_gen, weight, Nsolns);
                fillUnderOverFlow(hTrue_vs_Meas, sign(observable)*ttmass, sign(observable_gen)*ttmass_gen, weight, Nsolns);
                if ( combineLepMinus )
                {
                    fillUnderOverFlow(hMeas, sign(observableMinus)*ttmass, weight, Nsolns);
                    fillUnderOverFlow(hTrue, sign(observableMinus_gen)*ttmass_gen, weight, Nsolns);
                    fillUnderOverFlow(hTrue_vs_Meas, sign(observableMinus)*ttmass, sign(observableMinus_gen)*ttmass_gen, weight, Nsolns);
                }
            }
        }

        RooUnfoldResponse response (hMeas, hTrue, hTrue_vs_Meas);

        TCanvas *c_mtt = new TCanvas("c_mtt", "c_mtt", 500, 500);

        hData->SetLineWidth(lineWidth + 2);

        hTrue->SetLineColor(TColor::GetColorDark(kGreen));
        hTrue->SetFillColor(TColor::GetColorDark(kGreen));
        hTrue->SetFillStyle(3353);

        hMeas->SetLineColor(TColor::GetColorDark(kGreen));
        hMeas->SetFillColor(TColor::GetColorDark(kGreen));
        hMeas->SetFillStyle(3353);

        hBkg->SetLineColor(kYellow);
        hBkg->SetFillColor(kYellow);


        THStack *hs = new THStack("hs", "Stacked Top+BG");

        hs->Add(hBkg);
        hs->Add(hMeas);

        hs->SetMinimum(0.0);
        hs->SetMaximum( 2.0 * hs->GetMaximum());
        hs->Draw("hist");
        hs->GetXaxis()->SetTitle("M_{tt}");
        hs->GetYaxis()->SetTitleOffset(1.3);
        hs->GetYaxis()->SetTitle("Events");

        hData->Draw("E same");

        TLegend *leg1 = new TLegend(0.6, 0.62, 0.9, 0.838, NULL, "brNDC");
        leg1->SetEntrySeparation(100);
        leg1->SetFillColor(0);
        leg1->SetLineColor(0);
        leg1->SetBorderSize(0);
        leg1->SetTextSize(0.03);
        leg1->SetFillStyle(0);
        leg1->AddEntry(hData, "Data");
        leg1->AddEntry(hMeas,  "MC@NLO reco level", "F");
        leg1->AddEntry(hBkg,  "Background", "F");
        leg1->Draw();
        c_mtt->SaveAs("Mtt_" + acceptanceName + Region + ".pdf");


        hData_bkgSub = (TH1D *) hData->Clone();
        hData_bkgSub->Add(hBkg, -1.0);


        if (unfoldingType == 0)
        {
            RooUnfoldSvd unfold_svd (&response, hData_bkgSub, kterm);
            unfold_svd.Setup(&response, hData_bkgSub);
            unfold_svd.IncludeSystematics(includeSys);
            hData_unfolded = (TH1D *) unfold_svd.Hreco();
            m_unfoldE = unfold_svd.Ereco();
        }
        else if (unfoldingType == 1)
        {
            RooUnfoldTUnfold unfold_rooTUnfold (&response, hData_bkgSub, TUnfold::kRegModeCurvature);
            unfold_rooTUnfold.Setup(&response, hData_bkgSub);
            unfold_rooTUnfold.FixTau(tau);
            unfold_rooTUnfold.IncludeSystematics(includeSys);
            hData_unfolded = (TH1D *) unfold_rooTUnfold.Hreco();
            m_unfoldE = unfold_rooTUnfold.Ereco();
        }
        else if (unfoldingType == 2)
        {
            TUnfold unfold_TUnfold (hTrue_vs_Meas, TUnfold::kHistMapOutputVert, TUnfold::kRegModeCurvature);
            unfold_TUnfold.SetInput(hMeas);
            //Double_t biasScale=1.0;
            unfold_TUnfold.SetBias(hTrue);
            unfold_TUnfold.DoUnfold(tau);
            unfold_TUnfold.GetOutput(hData_unfolded);


            TH2D *ematrix = unfold_TUnfold.GetEmatrix("ematrix", "error matrix", 0, 0);
            for (Int_t cmi = 0; cmi < nbins2D; cmi++)
            {
                for (Int_t cmj = 0; cmj < nbins2D; cmj++)
                {
                    m_unfoldE(cmi, cmj) = ematrix->GetBinContent(cmi + 1, cmj + 1);
                }
            }
        }
        else cout << "Unfolding TYPE not Specified" << "\n";


        if (unfoldingType == 0)
        {
            TCanvas *c_d = new TCanvas("c_d", "c_d", 500, 500);
            TH1D *dvec = unfold_svd.Impl()->GetD();
            dvec->Draw();
            c_d->SetLogy();
            c_d->SaveAs("D_2D_" + acceptanceName + Region + ".pdf");
        }

        TCanvas *c_resp = new TCanvas("c_resp", "c_resp");
        TH2D *hResp = (TH2D *) response.Hresponse();
        gStyle->SetPalette(1);
        hResp->GetXaxis()->SetTitle("M_{tt} #times sign(" + xaxislabel + ") (reco)");
        hResp->GetYaxis()->SetTitle("M_{tt} #times sign(" + xaxislabel + ") (gen)");
        hResp->Draw("COLZ");
        c_resp->SetLogz();
        c_resp->SaveAs("Response_2D_" + acceptanceName + Region + ".pdf");


        TFile *file = new TFile("../acceptance/mcnlo/accept_" + acceptanceName + ".root");

        TH2D *acceptM_2d = (TH2D *) file->Get("accept_" + acceptanceName + "_mtt");

        TH1D *acceptM = new TH1D ("accept", "accept",    nbins2D, xbins2D);
        acceptM->SetBinContent(1, acceptM_2d->GetBinContent(1, 3));
        acceptM->SetBinContent(2, acceptM_2d->GetBinContent(1, 2));
        acceptM->SetBinContent(3, acceptM_2d->GetBinContent(1, 1));

        acceptM->SetBinContent(4, acceptM_2d->GetBinContent(2, 1));
        acceptM->SetBinContent(5, acceptM_2d->GetBinContent(2, 2));
        acceptM->SetBinContent(6, acceptM_2d->GetBinContent(2, 3));

        acceptM->Scale(1.0 / acceptM->Integral());


        TH2D *denomM_2d = (TH2D *) file->Get("denominator_" + acceptanceName + "_mtt");
        TH1D *denomM = new TH1D ("denom", "denom",    nbins2D, xbins2D);

        denomM->SetBinContent(1, denomM_2d->GetBinContent(1, 3));
        denomM->SetBinContent(2, denomM_2d->GetBinContent(1, 2));
        denomM->SetBinContent(3, denomM_2d->GetBinContent(1, 1));

        denomM->SetBinContent(4, denomM_2d->GetBinContent(2, 1));
        denomM->SetBinContent(5, denomM_2d->GetBinContent(2, 2));
        denomM->SetBinContent(6, denomM_2d->GetBinContent(2, 3));

        TH1D *denomM_0 = new TH1D ("denominator0", "denominator0",    2, -1500., 1500.);
        TH1D *denomM_1 = new TH1D ("denominator1", "denominator1",    2, -1500., 1500.);
        TH1D *denomM_2 = new TH1D ("denominator2", "denominator2",    2, -1500., 1500.);

        denomM_2->SetBinContent(1, denomM_2d->GetBinContent(1, 3));
        denomM_1->SetBinContent(1, denomM_2d->GetBinContent(1, 2));
        denomM_0->SetBinContent(1, denomM_2d->GetBinContent(1, 1));

        denomM_0->SetBinContent(2, denomM_2d->GetBinContent(2, 1));
        denomM_1->SetBinContent(2, denomM_2d->GetBinContent(2, 2));
        denomM_2->SetBinContent(2, denomM_2d->GetBinContent(2, 3));




        TFile *file_nopTreweighting = new TFile("../acceptance/mcnlo_nopTreweighting/accept_" + acceptanceName + ".root");

        TH2D *denomM_nopTreweighting_2d = (TH2D *) file_nopTreweighting->Get("denominator_" + acceptanceName + "_mtt");
        TH1D *denomM_nopTreweighting = new TH1D ("denomnopTreweighting", "denomnopTreweighting",    nbins2D, xbins2D);

        denomM_nopTreweighting->SetBinContent(1, denomM_nopTreweighting_2d->GetBinContent(1, 3));
        denomM_nopTreweighting->SetBinContent(2, denomM_nopTreweighting_2d->GetBinContent(1, 2));
        denomM_nopTreweighting->SetBinContent(3, denomM_nopTreweighting_2d->GetBinContent(1, 1));

        denomM_nopTreweighting->SetBinContent(4, denomM_nopTreweighting_2d->GetBinContent(2, 1));
        denomM_nopTreweighting->SetBinContent(5, denomM_nopTreweighting_2d->GetBinContent(2, 2));
        denomM_nopTreweighting->SetBinContent(6, denomM_nopTreweighting_2d->GetBinContent(2, 3));

        TH1D *denomM_nopTreweighting_0 = new TH1D ("denominator0nopTreweighting", "denominator0nopTreweighting",    2, -1500., 1500.);
        TH1D *denomM_nopTreweighting_1 = new TH1D ("denominator1nopTreweighting", "denominator1nopTreweighting",    2, -1500., 1500.);
        TH1D *denomM_nopTreweighting_2 = new TH1D ("denominator2nopTreweighting", "denominator2nopTreweighting",    2, -1500., 1500.);

        denomM_nopTreweighting_2->SetBinContent(1, denomM_nopTreweighting_2d->GetBinContent(1, 3));
        denomM_nopTreweighting_1->SetBinContent(1, denomM_nopTreweighting_2d->GetBinContent(1, 2));
        denomM_nopTreweighting_0->SetBinContent(1, denomM_nopTreweighting_2d->GetBinContent(1, 1));

        denomM_nopTreweighting_0->SetBinContent(2, denomM_nopTreweighting_2d->GetBinContent(2, 1));
        denomM_nopTreweighting_1->SetBinContent(2, denomM_nopTreweighting_2d->GetBinContent(2, 2));
        denomM_nopTreweighting_2->SetBinContent(2, denomM_nopTreweighting_2d->GetBinContent(2, 3));





        for (Int_t i = 1; i <= nbins2D; i++)
        {

            if (acceptM->GetBinContent(i) != 0)
            {
                hData_unfolded->SetBinContent(i, hData_unfolded->GetBinContent(i) * 1.0 / acceptM->GetBinContent(i));
                hData_unfolded->SetBinError  (i, hData_unfolded->GetBinError  (i) * 1.0 / acceptM->GetBinContent(i));
            }

            if (acceptM->GetBinContent(i) != 0)
            {
                hTrue->SetBinContent(i, hTrue->GetBinContent(i) * 1.0 / acceptM->GetBinContent(i));
                hTrue->SetBinError  (i, hTrue->GetBinError(i)  * 1.0 / acceptM->GetBinContent(i));
            }
        }


        for (int l = 0; l < nbins2D; l++)
        {
            for (int j = 0; j < nbins2D; j++)
            {
                double corr = 1.0 / ( acceptM->GetBinContent(l + 1) * acceptM->GetBinContent(j + 1) );
                //corr = corr * pow(xsection / dataIntegral,2) ;
                m_correctE(l, j) = m_unfoldE(l, j) * corr;
            }
        }


        //==================================================================
        // ============== Print the assymetry =============================
        cout << "========= Variable: " << acceptanceName << "===================\n";
        Float_t Afb, AfbErr;

        GetAfb(hData, Afb, AfbErr);
        cout << " Data: " << Afb << " +/-  " << AfbErr << "\n";

        GetAfb(hTrue, Afb, AfbErr);
        cout << " True Top: " << Afb << " +/-  " << AfbErr << "\n";

        GetCorrectedAfb(hData_unfolded, m_correctE, Afb, AfbErr);
        cout << " Unfolded: " << Afb << " +/-  " << AfbErr << "\n";
        second_output_file << acceptanceName << " " << observablename << " Unfolded: " << Afb << " +/-  " << AfbErr << endl;

        GetAfb(denomM, Afb, AfbErr);
        cout << " True Top from acceptance denominator: " << Afb << " +/-  " << AfbErr << "\n";
        second_output_file << acceptanceName << " " << observablename << " True_Top_from_acceptance_denominator: " << Afb << " +/-  " << AfbErr << "\n";

        vector<double> afb_m;
        vector<double> afb_merr;
        GetAvsY(hData_unfolded, m_correctE, afb_m, afb_merr, second_output_file);

        Float_t AfbG[3];

        GetAfb(denomM_0, AfbG[0], AfbErr);
        cout << " True Top 0 from acceptance denominator: " << AfbG[0] << " +/-  " << AfbErr << "\n";
        second_output_file << acceptanceName << " " << observablename << " True_Top_0_from_acceptance_denominator: " << AfbG[0] << " +/-  " << AfbErr << "\n";
        GetAfb(denomM_1, AfbG[1], AfbErr);
        cout << " True Top 1 from acceptance denominator: " << AfbG[1] << " +/-  " << AfbErr << "\n";
        second_output_file << acceptanceName << " " << observablename << " True_Top_1_from_acceptance_denominator: " << AfbG[1] << " +/-  " << AfbErr << "\n";
        GetAfb(denomM_2, AfbG[2], AfbErr);
        cout << " True Top 2 from acceptance denominator: " << AfbG[2] << " +/-  " << AfbErr << "\n";
        second_output_file << acceptanceName << " " << observablename << " True_Top_2_from_acceptance_denominator: " << AfbG[2] << " +/-  " << AfbErr << "\n";


        if (draw_truth_before_pT_reweighting)
        {
            GetAfb(denomM_nopTreweighting_0, AfbG[0], AfbErr);
            GetAfb(denomM_nopTreweighting_1, AfbG[1], AfbErr);
            GetAfb(denomM_nopTreweighting_2, AfbG[2], AfbErr);
        }


        TCanvas *c_afb = new TCanvas("c_afb", "c_afb", 500, 500);
        double xbins2D_positive[4] = {300.0, xbins2D[4], xbins2D[5], xbins2D[6]};
        TH1D *hAfbVsMtt = new TH1D ("AfbVsMtt",  "AfbVsMtt",  3, xbins2D_positive);
        TH1D *hAfbVsMtt_statonly = new TH1D ("AfbVsMtt_statonly",  "AfbVsMtt_statonly",  3, xbins2D_positive);
        for (int nb = 0; nb < 3; nb++)
        {
            hAfbVsMtt->SetBinContent(nb + 1, afb_m[nb]);
            if (checkErrors)
            {
                if (includeSys)
                {
                    cout << "Difference between calculated and hard-coded stat errors: " << afb_merr[nb] - stat_corr[nb] << endl;
                }
                else
                {
                    cout << "Difference between calculated and hard-coded stat errors: " << afb_merr[nb] - stat_uncorr[nb] << endl;
                }
            }
            //hAfbVsMtt->SetBinError(nb+1,afb_merr[nb]);
            hAfbVsMtt->SetBinError(nb + 1,  sqrt( pow(stat_corr[nb], 2) + pow(syst_corr[nb], 2) ) );
            hAfbVsMtt_statonly->SetBinContent(nb + 1, afb_m[nb]);
            hAfbVsMtt_statonly->SetBinError(nb + 1, stat_uncorr[nb]);
        }

        //  GetAvsY(hTrue, m_unfoldE, afb_m, afb_merr);

        TH1D *hTop_AfbVsMtt = new TH1D ("Top_AfbVsMtt",  "Top_AfbVsMtt",  3, xbins2D_positive);
        for (int nb = 0; nb < 3; nb++)
        {
            hTop_AfbVsMtt->SetBinContent(nb + 1, AfbG[nb]);
            hTop_AfbVsMtt->SetBinError(nb + 1, 0);
        }

        tdrStyle->SetErrorX(0.5);
        hAfbVsMtt->SetMinimum( hAfbVsMtt->GetMinimum() - 0.1 );
        hAfbVsMtt->SetMaximum( hAfbVsMtt->GetMaximum() + 0.1 );
        hAfbVsMtt->SetLineWidth( 2.0 );
        hAfbVsMtt->Draw("E");
        hAfbVsMtt_statonly->Draw("E1 same");
        hTop_AfbVsMtt->SetLineColor(TColor::GetColorDark(kRed));
        hTop_AfbVsMtt->SetMarkerColor(TColor::GetColorDark(kRed));
        hTop_AfbVsMtt->SetMarkerSize(0);
        hTop_AfbVsMtt->SetLineWidth( 2.0 );
        hAfbVsMtt->GetYaxis()->SetTitle(asymlabel);
        hAfbVsMtt->GetYaxis()->SetTitleOffset(1.2);
        hAfbVsMtt->GetXaxis()->SetTitle("M_{t#bar t} (GeV/c^{2})");
        hTop_AfbVsMtt->Draw("E same");

        leg1 = new TLegend(0.6, 0.72, 0.9, 0.938, NULL, "brNDC");
        leg1->SetEntrySeparation(100);
        leg1->SetFillColor(0);
        leg1->SetLineColor(0);
        leg1->SetBorderSize(0);
        leg1->SetTextSize(0.03);
        leg1->SetFillStyle(0);
        leg1->AddEntry(hAfbVsMtt, "data");
        leg1->AddEntry(hTop_AfbVsMtt,    "MC@NLO parton level");
        leg1->Draw();

        TPaveText *pt1 = new TPaveText(0.18, 0.88, 0.41, 0.91, "brNDC");
        pt1->SetName("pt1name");
        pt1->SetBorderSize(0);
        pt1->SetFillStyle(0);

        TText *blah;
        //blah = pt1->AddText("CMS Preliminary, 5.0 fb^{-1} at  #sqrt{s}=7 TeV");
        blah = pt1->AddText("CMS, 5.0 fb^{-1} at  #sqrt{s}=7 TeV");
        blah->SetTextSize(0.032);
        blah->SetTextAlign(11);
        pt1->Draw();

        c_afb->SaveAs("AfbVsMtt_unfolded_" + acceptanceName + Region + ".pdf");
        c_afb->SaveAs("AfbVsMtt_unfolded_" + acceptanceName + Region + ".root");
        c_afb->SaveAs("AfbVsMtt_unfolded_" + acceptanceName + Region + ".C");

        TCanvas *c_mttu = new TCanvas("c_mttu", "c_mttu", 500, 500);

        hData_unfolded->Scale(1. / hData_unfolded->Integral(), "width");
        hTrue->Scale(1. / hTrue->Integral(), "width");

        hData_unfolded->GetXaxis()->SetTitle("M_{t#bar t}");
        hData_unfolded->GetYaxis()->SetTitle("d#sigma/dM_{t#bar t}");
        hData_unfolded->SetMinimum(0.0);
        hData_unfolded->SetMaximum( 2.0 * hData_unfolded->GetMaximum());
        hData_unfolded->SetMarkerStyle(23);
        hData_unfolded->SetMarkerSize(2.0);
        hData_unfolded->Draw("E");
        hData_unfolded->SetLineWidth(lineWidth);
        hTrue->SetLineWidth(lineWidth);
        hTrue->SetLineColor(TColor::GetColorDark(kGreen));
        hTrue->SetFillColor(TColor::GetColorDark(kGreen));
        hTrue->Draw("hist same");
        hData_unfolded->Draw("E same");

        leg1 = new TLegend(0.6, 0.62, 0.9, 0.838, NULL, "brNDC");
        leg1->SetEntrySeparation(100);
        leg1->SetFillColor(0);
        leg1->SetLineColor(0);
        leg1->SetBorderSize(0);
        leg1->SetTextSize(0.03);
        leg1->SetFillStyle(0);
        leg1->AddEntry(hData_unfolded, "( Data-BG ) Unfolded");
        leg1->AddEntry(hTrue,    "MC@NLO parton level", "F");
        leg1->Draw();

        c_mttu->SaveAs("Mtt_2D_unfolded_" + acceptanceName + Region + ".pdf");

        ch_data->Delete();

        ch_top->Delete();

        for (int iBkg = 0; iBkg < nBkg; ++iBkg)
        {
            ch_bkg[iBkg]->Delete();
        }

    }
    myfile.close();
    second_output_file.close();
}

#ifndef __CINT__
int main ()
{
    AfbUnfoldExample();    // Main program when run stand-alone
    return 0;
}
#endif
