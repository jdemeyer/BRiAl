// 
//  fglm.cc
//  PolyBoRi
//  
//  Created by Michael Brickenstein on 2008-11-13.
//  Copyright 2008 The PolyBoRi Team.
// 

#include "fglm.h"
#include "nf.h"
#include "interpolate.h"
BEGIN_NAMESPACE_PBORIGB
void FGLMStrategy::setupStandardMonomialsFromTables(){
     ring_with_ordering_type backup_ring=BooleEnv::ring();
     BooleEnv::set(from);
     standardMonomialsFromVector.resize(varietySize);
     MonomialSet::const_iterator it_set=standardMonomialsFrom.begin();
     MonomialSet::const_iterator end_set=standardMonomialsFrom.end();
     //assume only that iteration is descending w.r.t. divisibility
     
     int i=standardMonomialsFrom.size()-1;
     while(it_set!=end_set){
         Monomial m=*it_set;
         standardMonomialsFrom2Index[m]=i;
         standardMonomialsFromVector[i]=m;
         it_set++;
         i--;
     }

     BooleEnv::set(backup_ring);
     
}
void FGLMStrategy::writeTailToRow(MonomialSet tail, packedmatrix* row){
    MonomialSet::const_iterator it=tail.begin();
    MonomialSet::const_iterator end=tail.end();
            //optimize that;
    while(it!=end){
        idx_type tail_idx=standardMonomialsFrom2Index[*it];
        mzd_write_bit(row,0, tail_idx,1);
        it++;
    }
}
void FGLMStrategy::writeRowToVariableDivisors(packedmatrix* row, Monomial lm){
    Monomial::const_iterator it_lm=lm.begin();
    Monomial::const_iterator end_lm=lm.end();
    bool first=true;
    while(it_lm!=end_lm){
        idx_type ring_var_index=*it_lm;
        idx_type our_var_index=ring2Index[ring_var_index];
        Monomial divided=lm/Variable(ring_var_index);
        if (standardMonomialsFrom.owns(divided)){
            packedmatrix* mat=multiplicationTables[our_var_index];
            size_t divided_index=standardMonomialsFrom2Index[divided];

            if (first){
                monomial2MultiplicationMatrix[lm]=our_var_index;
                monomial2MultiplicationMatrixRowIndex[lm]=divided_index;
                first=false;
            }
            int j;
            for(j=0;j<varietySize;j++){
                mzd_write_bit(mat, divided_index, j, mzd_read_bit(row,0,j));
            }
        }
        it_lm++;
    }
}




void FGLMStrategy::setupMultiplicationTables(){
    ring_with_ordering_type backup_ring=BooleEnv::ring();
    BooleEnv::set(from);
    
    //first we write into rows, later we transpose
    //algorithm here
    int i;
    multiplicationTables.resize(nVariables);
    for(i=0;i<nVariables;i++){
        multiplicationTables[i]=mzd_init(varietySize,varietySize);
    }
    
    //standard monomials
    
    for(i=0;i<standardMonomialsFromVector.size();i++){
        Monomial m=standardMonomialsFromVector[i];
        Monomial::const_iterator it=m.begin();
        Monomial::const_iterator end=m.end();
        while(it!=end){
            idx_type ring_var_index=*it;
            idx_type our_var_index=ring2Index[ring_var_index];
            Monomial divided=m/Variable(ring_var_index);
            size_t divided_index=standardMonomialsFrom2Index[divided];
            packedmatrix* mat=multiplicationTables[our_var_index];
            mzd_write_bit(mat, divided_index,i, 1);
            it++;
        }
    }
    
    //leading monomials from gb: vertices/
    packedmatrix* row=mzd_init(1, varietySize);
    for(i=0;i<gbFrom.size();i++){
        Monomial lm=gbFrom[i].lm;
        MonomialSet tail=gbFrom[i].tail.diagram();
        mzd_row_clear_offset(row,0,0);
        writeTailToRow(tail, row);
        writeRowToVariableDivisors(row,lm);
        
    }
    mzd_free(row);
    //edges
    MonomialSet edges=standardMonomialsFrom.cartesianProduct(varsSet).
        diff(standardMonomialsFrom).diff(leadingTermsFrom);
    MonomialVector edges_vec(edges.size());
    std::copy(edges.begin(), edges.end(), edges_vec.begin());
    
    //reverse is important, so that divisors have already been treated
    
    MonomialVector::reverse_iterator it_edges=edges_vec.rbegin();
    MonomialVector::reverse_iterator end_edges=edges_vec.rend();
    MonomialSet EdgesUnitedVertices=edges.unite(leadingTermsFrom);
    
    packedmatrix* multiplied_row=mzd_init(1,varietySize);
    while(it_edges!=end_edges){
        mzd_row_clear_offset(multiplied_row, 0, 0);
        Monomial m=*it_edges;
        MonomialSet candidates=Polynomial(EdgesUnitedVertices.divisorsOf(m)).gradedPart(m.deg()-1).set();
        
        Monomial reduced_problem_to=*(candidates.begin());
        Monomial v_m=m/reduced_problem_to;
        assert (v_m.deg()==1);
        Variable var=*v_m.variableBegin();
        packedmatrix* mult_table=multiplicationTableForVariable(var);
        
        packedmatrix* window=findVectorInMultTables(reduced_problem_to);
        
        //standardMonomialsFrom2Index[reduced_problem_to];
        
        //highly inefficient/far to many allocations
        
        //mzd_mul expects second arg to be transposed
        //which is a little bit tricky as we multiply from left
        packedmatrix* transposed_mult_table=mzd_transpose(NULL, mult_table);
        mzd_mul_naiv(multiplied_row, window, transposed_mult_table);
        
        writeRowToVariableDivisors(multiplied_row, m);
        //matrices are transposed, so now we have write to columns
        
        mzd_free(transposed_mult_table);
        mzd_free_window(window);
        it_edges++;
    }
   
    mzd_free(multiplied_row);
    
    
    
    //From now on, we multiply, so here we transpose
    for(i=0;i<multiplicationTables.size();i++){
        //unnecassary many allocations of matrices
        packedmatrix* new_mat=mzd_init(varietySize,varietySize);
        mzd_transpose(new_mat, multiplicationTables[i]);
        mzd_free(multiplicationTables[i]);
        multiplicationTables[i]=new_mat;
    }
    
    
    BooleEnv::set(backup_ring);
}
void FGLMStrategy::analyzeGB(const ReductionStrategy& gb){
    ring_with_ordering_type backup_ring=BooleEnv::ring();
    BooleEnv::set(from);
    vars=gb.leadingTerms.usedVariables();
    
    Monomial::variable_iterator it_var=vars.variableBegin();
    Monomial::variable_iterator end_var=vars.variableEnd();
    while (it_var!=end_var){
        varsVector.push_back(*it_var);
        it_var++;
    }
    VariableVector::reverse_iterator it_varvec=varsVector.rbegin();
    VariableVector::reverse_iterator end_varvec=varsVector.rend();
    while(it_varvec!=end_varvec){
        varsSet=varsSet.unite(Monomial(*it_varvec).diagram());
        it_varvec++;
    }

    int i;
    for (i=0;i<gb.size();i++){
        vars=vars * Monomial(gb[i].usedVariables,BooleEnv::ring());
    }
    nVariables=vars.deg();
    ring2Index.resize(BooleEnv::ring().nVariables());
    index2Ring.resize(nVariables);
    idx_type ring_index;
    idx_type our_index=0;
    Monomial::const_iterator it=vars.begin();
    Monomial::const_iterator end=vars.end();
    while(it!=end){
        ring_index=*it;
        ring2Index[ring_index]=our_index;
        index2Ring[our_index]=ring_index;
        
        our_index++;
        it++;
    }
    
    standardMonomialsFrom=mod_mon_set(vars.divisors(), gb.leadingTerms);
    
    leadingTermsFrom=gb.leadingTerms;
    varietySize=standardMonomialsFrom.size();
    BooleEnv::set(backup_ring);
}
PolynomialVector FGLMStrategy::main(){
    PolynomialVector res;
    return res;
}
Polynomial FGLMStrategy::reducedNormalFormInFromRing(Polynomial f){
    ring_with_ordering_type bak_ring=BooleEnv::ring();
    BooleEnv::set(to);
    Polynomial res=gbFrom.reducedNormalForm(f);
    BooleEnv::set(bak_ring);
    return res;
    
}
END_NAMESPACE_PBORIGB